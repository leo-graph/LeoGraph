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

void lowerSubquerySource(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    PlanScope & scope,
    std::string_view context_name)
{
    if (subquery.at_schema)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} AT schema is not supported", context_name);
    if (subquery.bindings)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings are not supported", context_name);
    if (!subquery.next_statements.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} NEXT statements are not supported", context_name);

    const auto * single_query = subquery.query ? subquery.query->as<GAST::GQLSingleQuery>() : nullptr;
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain a single query", context_name);

    PlanBuilder nested_builder(context, scope.makeChildGraphScope());
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
    if (call.variable_scope)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope is not supported");

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
