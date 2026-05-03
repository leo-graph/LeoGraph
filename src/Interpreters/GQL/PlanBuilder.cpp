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

    if (!query.clauses.front()->as<GAST::GQLMatchClause>())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL queries starting with MATCH are supported");

    std::vector<const GAST::GQLMatchClause *> pending_matches;
    bool match_sequence_closed = false;
    for (const auto & clause : query.clauses)
    {
        if (const auto * match = clause->as<GAST::GQLMatchClause>())
        {
            if (match_sequence_closed)
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "MATCH clauses after non-MATCH clauses are not supported");

            pending_matches.push_back(match);
            continue;
        }

        if (!pending_matches.empty())
        {
            lowerMatchClauseSequence(plan, pending_matches, context);
            pending_matches.clear();
            match_sequence_closed = true;
        }

        lowerPipelineClause(plan, clause, context);
    }

    if (!pending_matches.empty())
        lowerMatchClauseSequence(plan, pending_matches, context);
}

}

}
