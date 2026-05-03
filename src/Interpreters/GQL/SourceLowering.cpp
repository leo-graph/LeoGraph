#include <Interpreters/GQL/SourceLowering.h>

#include <Core/Block.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchLowering.h>
#include <Interpreters/GQL/PlanBuilder.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/Chunk.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Processors/QueryPlan/ReadFromPreparedSource.h>
#include <Processors/Sources/SourceFromSingleChunk.h>
#include <QueryPipeline/Pipe.h>

#include <memory>
#include <unordered_set>
#include <vector>

namespace DB::ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

ASTPtr cloneOrNull(const ASTPtr & ast)
{
    return ast ? ast->clone() : nullptr;
}

void addEmptySingleRowSource(QueryPlan & plan, PlanScope & scope)
{
    auto header = std::make_shared<const Block>();
    Columns columns;
    auto source = std::make_shared<SourceFromSingleChunk>(header, Chunk(std::move(columns), 1));
    plan.addStep(std::make_unique<ReadFromPreparedSource>(Pipe(std::move(source))));
    scope.replaceWithHeader(*plan.getCurrentHeader(), BindingKind::Source);
}

ASTPtr makeValueBindingExpression(const GAST::GQLBindingVariableDefinition & definition, std::string_view context_name)
{
    const auto * initializer = definition.initializer ? definition.initializer->as<GAST::GQLBindingInitializer>() : nullptr;
    if (!initializer)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} VALUE binding '{}' must have an initializer", context_name, definition.name);
    if (initializer->kind != GAST::GQLBindingInitializer::Kind::Value)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding '{}' must have a VALUE initializer", context_name, definition.name);
    if (!initializer->value)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} VALUE binding '{}' must have a value expression", context_name, definition.name);

    auto expression = initializer->value->clone();
    if (definition.type)
        expression = GAST::GQLExpr::castExpr(std::move(expression), definition.type->clone());

    return expression;
}

void lowerSubqueryBindings(const GAST::GQLBindingVariableDefinitionBlock & block, PlanScope & scope, std::string_view context_name)
{
    std::unordered_set<String> names;
    for (const auto & definition_ast : block.definitions)
    {
        const auto * definition = definition_ast ? definition_ast->as<GAST::GQLBindingVariableDefinition>() : nullptr;
        if (!definition)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding must be GQLBindingVariableDefinition", context_name);
        if (definition->name.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding name must be non-empty", context_name);
        if (!names.insert(definition->name).second)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Duplicate {} binding '{}'", context_name, definition->name);
        if (scope.hasBinding(definition->name))
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding '{}' conflicts with an outer binding", context_name, definition->name);

        if (definition->kind != GAST::GQLBindingVariableDefinition::Kind::Value)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} only supports VALUE bindings", context_name);

        scope.addOrReplaceBinding(
            definition->name,
            nullptr,
            BindingKind::Projection,
            makeValueBindingExpression(*definition, context_name));
    }
}

void validateInlineCallVariableScope(const GAST::GQLCallInlineClause & call)
{
    if (!call.variable_scope)
        return;

    const auto * variable_scope = call.variable_scope->as<GAST::GQLCallVariableScopeClause>();
    if (!variable_scope)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope must be GQLCallVariableScopeClause");

    if (!variable_scope->variables.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope imports are not supported");
}

void lowerSubquerySource(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    PlanScope & scope,
    std::string_view context_name)
{
    if (subquery.at_schema)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} AT schema is not supported", context_name);
    if (!subquery.next_statements.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} NEXT statements are not supported", context_name);

    const auto * single_query = subquery.query ? subquery.query->as<GAST::GQLSingleQuery>() : nullptr;
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain a single query", context_name);

    auto child_scope = scope.makeChildGraphScope();
    if (subquery.bindings)
    {
        const auto * binding_block = subquery.bindings->as<GAST::GQLBindingVariableDefinitionBlock>();
        if (!binding_block)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings must be GQLBindingVariableDefinitionBlock", context_name);

        lowerSubqueryBindings(*binding_block, child_scope, context_name);
    }

    PlanBuilder nested_builder(context, std::move(child_scope));
    nested_builder.buildSingleQuery(plan, *single_query);
    scope = nested_builder.getScope();
}

void lowerSelectSource(QueryPlan & plan, const ASTPtr & source, ContextPtr context, PlanScope & scope)
{
    if (!source)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "SELECT source is null");

    if (const auto * match = source->as<GAST::GQLMatchClause>())
    {
        std::vector<const GAST::GQLMatchClause *> matches{match};
        lowerMatchClauseSequence(plan, matches, context, scope);
        return;
    }

    if (const auto * subquery = source->as<GAST::GQLSubquery>())
    {
        lowerSubquerySource(plan, *subquery, context, scope, "SELECT FROM subquery");
        return;
    }

    if (const auto * source_item = source->as<GAST::GQLSelectSourceItem>())
    {
        if (source_item->graph_reference)
        {
            auto previous_graph = cloneOrNull(scope.getActiveGraph());
            PlanScope source_scope = scope;
            source_scope.setActiveGraph(source_item->graph_reference->clone());

            lowerSelectSource(plan, source_item->source, context, source_scope);

            scope = std::move(source_scope);
            scope.setActiveGraph(std::move(previous_graph));
            return;
        }

        lowerSelectSource(plan, source_item->source, context, scope);
        return;
    }

    if (const auto * source_list = source->as<GAST::GQLSelectSourceList>())
    {
        if (source_list->items.size() != 1)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists are not supported");

        lowerSelectSource(plan, source_list->items.front(), context, scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported SELECT source: {}", source->getID(' '));
}

void lowerInlineCallSource(QueryPlan & plan, const GAST::GQLCallInlineClause & call, ContextPtr context, PlanScope & scope)
{
    if (call.optional)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL inline CALL is not supported");

    validateInlineCallVariableScope(call);

    const auto * subquery = call.subquery ? call.subquery->as<GAST::GQLSubquery>() : nullptr;
    if (!subquery)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL must contain a subquery");

    lowerSubquerySource(plan, *subquery, context, scope, "inline CALL subquery");
}

}

void lowerUseClause(const OPENGQL::AST::GQLUseClause & use, PlanScope & scope)
{
    if (!use.graph_reference)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL USE must contain a graph reference");

    scope.setActiveGraph(use.graph_reference->clone());
}

bool SourceClauseBuffer::tryAppend(const ASTPtr & clause)
{
    if (const auto * match = clause->as<OPENGQL::AST::GQLMatchClause>())
    {
        match_clauses.push_back(match);
        return true;
    }

    return false;
}

bool SourceClauseBuffer::hasPending() const
{
    return !match_clauses.empty();
}

void SourceClauseBuffer::flush(QueryPlan & plan, ContextPtr context, PlanScope & scope)
{
    if (match_clauses.empty())
        return;

    lowerMatchClauseSequence(plan, match_clauses, context, scope);
    match_clauses.clear();
}

bool tryLowerStandaloneSourceClause(QueryPlan & plan, const ASTPtr & clause, ContextPtr context, PlanScope & scope)
{
    if (const auto * ret = clause->as<GAST::GQLReturnClause>())
    {
        addEmptySingleRowSource(plan, scope);
        lowerReturnClause(plan, *ret, context, scope);
        return true;
    }

    if (const auto * select = clause->as<GAST::GQLSelectClause>())
    {
        if (select->source)
        {
            lowerSelectSource(plan, select->source, context, scope);
            lowerSelectClause(plan, *select, context, scope, true);
        }
        else
        {
            addEmptySingleRowSource(plan, scope);
            lowerSelectClause(plan, *select, context, scope);
        }

        return true;
    }

    if (clause->as<GAST::GQLLetClause>())
    {
        addEmptySingleRowSource(plan, scope);
        lowerPipelineClause(plan, clause, context, scope);
        return true;
    }

    if (clause->as<GAST::GQLForClause>())
    {
        addEmptySingleRowSource(plan, scope);
        lowerPipelineClause(plan, clause, context, scope);
        return true;
    }

    if (const auto * inline_call = clause->as<GAST::GQLCallInlineClause>())
    {
        lowerInlineCallSource(plan, *inline_call, context, scope);
        return true;
    }

    return false;
}

}
