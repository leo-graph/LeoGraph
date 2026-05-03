#include <Interpreters/GQL/MatchLowering.h>

#include <memory>
#include <utility>

#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchPlanToSpec.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Interpreters/GQL/PatternLowering.h>
#include <Parsers/IAST.h>
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

void validateExecutableMatch(const Graph::MatchSpec & match)
{
    if (match.optional || match.has_optional_operand_block)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL MATCH is not supported");
    if (match.paths.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH must contain at least one path pattern");
}

}

void lowerMatchClauseSequence(
    QueryPlan & plan,
    const std::vector<const OPENGQL::AST::GQLMatchClause *> & matches,
    ContextPtr context,
    PlanScope & scope)
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
    if (scope.getActiveGraph())
        match_spec.graph_reference = scope.getActiveGraph()->clone();

    validateExecutableMatch(match_spec);
    plan.addStep(std::make_unique<Graph::MatchStep>(std::move(match_spec), scope.getMatchSourceFactory()));
    scope.replaceWithHeader(*plan.getCurrentHeader(), BindingKind::Source);

    for (const auto & match_plan : match_plans)
    {
        if (match_plan.where)
            lowerWhereClause(plan, *match_plan.where, context, "GQL MATCH", scope);
    }
}

}

}
