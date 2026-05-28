#include <Interpreters/GQL/PostSourceClausePlanner.h>

#include <Interpreters/GQL/CallPlanner.h>
#include <Interpreters/GQL/CatalogPlanner.h>
#include <Interpreters/GQL/ClausePlanner.h>
#include <Interpreters/GQL/MutationPlanner.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB::GQL
{

void planPostSourceClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (tryPlanCatalogClause(plan, clause, context, environment, scope))
        return;

    if (tryPlanDataModifyingClause(plan, clause, context, environment, scope))
        return;

    if (tryPlanPostSourceCallClause(plan, clause, context, environment, scope))
        return;

    planClauseTransform(plan, clause, context, scope);
}

}
