#include <Interpreters/GQL/PlanBuilder.h>

#include <memory>
#include <utility>

#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchPlanToSpec.h>
#include <Interpreters/GQL/PatternLowering.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/Graph/MatchStep.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Common/Exception.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

void validateExecutableMatch(const Graph::MatchSpec & match)
{
    if (match.optional || match.has_optional_operand_block)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL MATCH is not supported");
    if (match.paths.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH must contain at least one path pattern");
}

}

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
            lowerMatchSequence(plan, pending_matches);
            pending_matches.clear();
            match_sequence_closed = true;
        }

        lowerPipelineClause(plan, clause, context);
    }

    if (!pending_matches.empty())
        lowerMatchSequence(plan, pending_matches);
}

void PlanBuilder::lowerMatchSequence(QueryPlan & plan, const std::vector<const GAST::GQLMatchClause *> & matches) const
{
    std::vector<MatchPlan> match_plans;
    match_plans.reserve(matches.size());
    for (const auto * match : matches)
    {
        if (!match)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL MATCH clause node is null");

        match_plans.push_back(lowerMatchClause(*match));
    }

    auto match_spec = makeMatchSpec(match_plans);
    validateExecutableMatch(match_spec);
    plan.addStep(std::make_unique<Graph::MatchStep>(std::move(match_spec)));

    for (const auto & match_plan : match_plans)
    {
        if (match_plan.where)
            lowerWhereClause(plan, *match_plan.where, context, "GQL MATCH");
    }
}

}

}
