#include <Interpreters/GQL/SourceLowering.h>

#include <Core/Block.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchLowering.h>
#include <Interpreters/GQL/PlanBuilder.h>
#include <Interpreters/GQL/PlanEnvironment.h>
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

Names extractInlineCallVariableScope(const GAST::GQLCallInlineClause & call)
{
    Names imports;
    if (!call.variable_scope)
        return imports;

    const auto * variable_scope = call.variable_scope->as<GAST::GQLCallVariableScopeClause>();
    if (!variable_scope)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope must be GQLCallVariableScopeClause");

    std::unordered_set<String> seen;
    imports.reserve(variable_scope->variables.size());
    for (const auto & variable_ast : variable_scope->variables)
    {
        const auto * variable = variable_ast ? variable_ast->as<GAST::GQLExpr>() : nullptr;
        if (!variable || variable->kind != GAST::GQLExpr::Kind::Identifier || variable->text.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope must contain binding-variable identifiers");
        if (!seen.insert(variable->text).second)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Duplicate inline CALL variable scope import '{}'", variable->text);

        imports.push_back(variable->text);
    }

    return imports;
}

PlanScope makeInlineCallChildScope(const PlanScope & scope, const Names & imports)
{
    auto child_scope = scope.makeChildGraphScope();
    for (const auto & name : imports)
    {
        const auto * binding = scope.tryGetBinding(name);
        if (!binding)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' is not available", name);
        if (!binding->expression)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' requires correlation semantics", name);

        child_scope.addOrReplaceBinding(name, binding->type, binding->kind, binding->expression->clone());
    }

    return child_scope;
}

PlanScope makeInlineCallPipelineScope(const PlanScope & scope, const Names & imports)
{
    auto child_scope = scope.makeChildGraphScope();
    for (const auto & name : imports)
    {
        const auto * binding = scope.tryGetBinding(name);
        if (!binding)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' is not available", name);

        child_scope.addOrReplaceBinding(
            name,
            binding->type,
            binding->kind,
            binding->expression ? binding->expression->clone() : ASTPtr{});
    }

    return child_scope;
}

void lowerSubquerySource(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    PlanScope child_scope,
    std::string_view context_name)
{
    if (subquery.at_schema)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} AT schema is not supported", context_name);
    if (!subquery.next_statements.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} NEXT statements are not supported", context_name);

    const auto * single_query = subquery.query ? subquery.query->as<GAST::GQLSingleQuery>() : nullptr;
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain a single query", context_name);

    if (subquery.bindings)
    {
        const auto * binding_block = subquery.bindings->as<GAST::GQLBindingVariableDefinitionBlock>();
        if (!binding_block)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings must be GQLBindingVariableDefinitionBlock", context_name);

        lowerSubqueryBindings(*binding_block, child_scope, context_name);
    }

    PlanBuilder nested_builder(context, std::move(child_scope), environment);
    nested_builder.buildSingleQuery(plan, *single_query);
    scope = nested_builder.getScope();
}

void lowerPipelineOnlySubquery(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    std::string_view context_name);

bool sameGraphReference(const IAST & lhs_ast, const IAST & rhs_ast)
{
    const auto * lhs = lhs_ast.as<GAST::GQLGraphExpression>();
    const auto * rhs = rhs_ast.as<GAST::GQLGraphExpression>();
    if (!lhs || !rhs)
        return false;

    if (lhs->kind != rhs->kind || lhs->text != rhs->text)
        return false;

    return !lhs->value && !rhs->value;
}

void lowerGraphMatchSourceList(
    QueryPlan & plan,
    const GAST::GQLSelectSourceList & source_list,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (source_list.items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list must contain at least one source");

    std::vector<const GAST::GQLMatchClause *> matches;
    matches.reserve(source_list.items.size());

    const ASTPtr * graph_reference = nullptr;
    for (const auto & item_ast : source_list.items)
    {
        const auto * item = item_ast ? item_ast->as<GAST::GQLSelectSourceItem>() : nullptr;
        if (!item)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list item must be GQLSelectSourceItem");
        if (!item->graph_reference)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM graph match source must have a graph reference");

        if (!graph_reference)
        {
            graph_reference = &item->graph_reference;
        }
        else if (!sameGraphReference(**graph_reference, *item->graph_reference))
        {
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists with different graph references require source composition");
        }

        const auto * match = item->source ? item->source->as<GAST::GQLMatchClause>() : nullptr;
        if (!match)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists only support graph MATCH sources");

        matches.push_back(match);
    }

    auto previous_graph = cloneOrNull(scope.getActiveGraph());
    PlanScope source_scope = scope;
    source_scope.setActiveGraph((*graph_reference)->clone());

    lowerMatchClauseSequence(plan, matches, context, environment, source_scope);

    scope = std::move(source_scope);
    scope.setActiveGraph(std::move(previous_graph));
}

void lowerSubquerySource(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    std::string_view context_name)
{
    lowerSubquerySource(plan, subquery, context, environment, scope, scope.makeChildGraphScope(), context_name);
}

void lowerSelectSource(QueryPlan & plan, const ASTPtr & source, ContextPtr context, const PlanEnvironment & environment, PlanScope & scope)
{
    if (!source)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "SELECT source is null");

    if (const auto * match = source->as<GAST::GQLMatchClause>())
    {
        std::vector<const GAST::GQLMatchClause *> matches{match};
        lowerMatchClauseSequence(plan, matches, context, environment, scope);
        return;
    }

    if (const auto * subquery = source->as<GAST::GQLSubquery>())
    {
        lowerSubquerySource(plan, *subquery, context, environment, scope, "SELECT FROM subquery");
        return;
    }

    if (const auto * source_item = source->as<GAST::GQLSelectSourceItem>())
    {
        if (source_item->graph_reference)
        {
            auto previous_graph = cloneOrNull(scope.getActiveGraph());
            PlanScope source_scope = scope;
            source_scope.setActiveGraph(source_item->graph_reference->clone());

            lowerSelectSource(plan, source_item->source, context, environment, source_scope);

            scope = std::move(source_scope);
            scope.setActiveGraph(std::move(previous_graph));
            return;
        }

        lowerSelectSource(plan, source_item->source, context, environment, scope);
        return;
    }

    if (const auto * source_list = source->as<GAST::GQLSelectSourceList>())
    {
        if (source_list->items.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list must contain at least one source");

        if (source_list->items.size() > 1)
        {
            lowerGraphMatchSourceList(plan, *source_list, context, environment, scope);
            return;
        }

        lowerSelectSource(plan, source_list->items.front(), context, environment, scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported SELECT source: {}", source->getID(' '));
}

void lowerInlineCallSource(
    QueryPlan & plan,
    const GAST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (call.optional)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL inline CALL is not supported");

    const auto * subquery = call.subquery ? call.subquery->as<GAST::GQLSubquery>() : nullptr;
    if (!subquery)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL must contain a subquery");

    lowerSubquerySource(
        plan,
        *subquery,
        context,
        environment,
        scope,
        makeInlineCallChildScope(scope, extractInlineCallVariableScope(call)),
        "inline CALL subquery");
}

void lowerInlineCallPipelineClauseImpl(
    QueryPlan & plan,
    const GAST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (call.optional)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL inline CALL is not supported");

    const auto * subquery = call.subquery ? call.subquery->as<GAST::GQLSubquery>() : nullptr;
    if (!subquery)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL must contain a subquery");

    auto child_scope = makeInlineCallPipelineScope(scope, extractInlineCallVariableScope(call));
    lowerPipelineOnlySubquery(plan, *subquery, context, environment, child_scope, "pipeline inline CALL subquery");
    scope = std::move(child_scope);
}

void lowerPipelineOnlySubquery(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
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
    if (single_query->clauses.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain at least one clause", context_name);

    if (subquery.bindings)
    {
        const auto * binding_block = subquery.bindings->as<GAST::GQLBindingVariableDefinitionBlock>();
        if (!binding_block)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings must be GQLBindingVariableDefinitionBlock", context_name);

        lowerSubqueryBindings(*binding_block, scope, context_name);
    }

    for (const auto & clause : single_query->clauses)
    {
        if (const auto * use = clause->as<GAST::GQLUseClause>())
        {
            lowerUseClause(*use, scope);
            continue;
        }

        if (clause->as<GAST::GQLMatchClause>())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} source clause is not supported", context_name);

        if (const auto * select = clause->as<GAST::GQLSelectClause>(); select && select->source)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} source clause is not supported", context_name);

        if (const auto * inline_call = clause->as<GAST::GQLCallInlineClause>())
        {
            lowerInlineCallPipelineClauseImpl(plan, *inline_call, context, environment, scope);
            continue;
        }

        lowerPipelineClause(plan, clause, context, scope);
    }
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

void SourceClauseBuffer::flush(QueryPlan & plan, ContextPtr context, const PlanEnvironment & environment, PlanScope & scope)
{
    if (match_clauses.empty())
        return;

    lowerMatchClauseSequence(plan, match_clauses, context, environment, scope);
    match_clauses.clear();
}

bool tryLowerStandaloneSourceClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
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
            lowerSelectSource(plan, select->source, context, environment, scope);
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

    if (clause->as<GAST::GQLFinishClause>())
    {
        addEmptySingleRowSource(plan, scope);
        lowerPipelineClause(plan, clause, context, scope);
        return true;
    }

    if (const auto * inline_call = clause->as<GAST::GQLCallInlineClause>())
    {
        lowerInlineCallSource(plan, *inline_call, context, environment, scope);
        return true;
    }

    return false;
}

void lowerInlineCallPipelineClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    lowerInlineCallPipelineClauseImpl(plan, call, context, environment, scope);
}

}
