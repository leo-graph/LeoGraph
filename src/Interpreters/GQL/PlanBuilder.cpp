#include <Interpreters/GQL/PlanBuilder.h>

#include <utility>
#include <vector>

#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchLowering.h>
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

PlanBuilder::PlanBuilder(ContextPtr context_)
    : context(std::move(context_))
{
}

void PlanBuilder::buildSingleQuery(QueryPlan & plan, const GAST::GQLSingleQuery & query)
{
    if (query.clauses.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL query must contain at least one clause");

    std::vector<const GAST::GQLMatchClause *> pending_matches;
    bool source_ready = false;
    for (const auto & clause : query.clauses)
    {
        if (const auto * match = clause->as<GAST::GQLMatchClause>())
        {
            if (source_ready)
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "MATCH clauses after pipeline clauses are not supported");

            pending_matches.push_back(match);
            continue;
        }

        if (!pending_matches.empty())
        {
            lowerMatchClauseSequence(plan, pending_matches, context);
            pending_matches.clear();
            source_ready = true;
        }

        if (!source_ready)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL clause before a source clause is not supported: {}", clause->getID(' '));

        lowerPipelineClause(plan, clause, context);
    }

    if (!pending_matches.empty())
        lowerMatchClauseSequence(plan, pending_matches, context);
}

}

}
