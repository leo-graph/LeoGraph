#include <Interpreters/GQL/PlanBuilder.h>

#include <utility>

#include <Interpreters/GQL/CallLowering.h>
#include <Interpreters/GQL/CatalogLowering.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MutationLowering.h>
#include <Interpreters/GQL/SourceLowering.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Common/Exception.h>

namespace DB
{

namespace ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace GQL
{
namespace GAST = DB::OPENGQL::AST;

PlanBuilder::PlanBuilder(ContextPtr context_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
{
}

PlanBuilder::PlanBuilder(ContextPtr context_, PlanScope scope_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
    , scope(std::move(scope_))
{
}

void PlanBuilder::buildSingleQuery(QueryPlan & plan, const GAST::GQLSingleQuery & query)
{
    if (query.clauses.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL query must contain at least one clause");

    SourceClauseBuffer source_buffer;
    bool source_ready = false;
    for (const auto & clause : query.clauses)
    {
        if (const auto * use = clause->as<GAST::GQLUseClause>())
        {
            if (source_ready || source_buffer.hasPending())
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL USE after a source clause is not supported");

            lowerUseClause(*use, scope);
            continue;
        }

        if (source_buffer.tryAppend(clause))
        {
            if (source_ready)
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL source clauses after pipeline clauses are not supported");

            continue;
        }

        if (source_buffer.hasPending())
        {
            source_buffer.flush(plan, context, environment, scope);
            source_ready = true;
        }

        if (!source_ready)
        {
            if (isCatalogClause(clause))
            {
                lowerCatalogClause(plan, clause, context, scope);
                source_ready = true;
                continue;
            }

            if (isDataModifyingClause(clause))
            {
                lowerDataModifyingClause(plan, clause, context, scope);
                source_ready = true;
                continue;
            }

            if (tryLowerStandaloneCallClause(plan, clause, context, environment, scope))
            {
                source_ready = true;
                continue;
            }

            if (tryLowerStandaloneSourceClause(plan, clause, context, environment, scope))
            {
                source_ready = true;
                continue;
            }

            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL clause before a source clause is not supported: {}", clause->getID(' '));
        }

        if (isSourceIntroducingClause(clause))
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL source clauses after pipeline clauses are not supported");

        if (tryLowerPipelineCallClause(plan, clause, context, environment, scope))
            continue;

        lowerPipelineClause(plan, clause, context, scope);
    }

    if (source_buffer.hasPending())
        source_buffer.flush(plan, context, environment, scope);
}

}

}
