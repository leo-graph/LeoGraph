#include <Interpreters/GQL/SourceLowering.h>

#include <Core/Block.h>
#include <Interpreters/GQL/CallLowering.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Interpreters/GQL/SourceCompositionLowering.h>
#include <Interpreters/GQL/SubqueryLowering.h>
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

void lowerSubquerySourceWithChildGraphScope(
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
        lowerSubquerySourceWithChildGraphScope(plan, *subquery, context, environment, scope, "SELECT FROM subquery");
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
            lowerSelectSourceList(plan, *source_list, context, environment, scope);
            return;
        }

        lowerSelectSource(plan, source_list->items.front(), context, environment, scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported SELECT source: {}", source->getID(' '));
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

}
