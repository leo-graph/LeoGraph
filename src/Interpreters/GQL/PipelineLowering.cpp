#include <Interpreters/GQL/PipelineLowering.h>

#include <Interpreters/GQL/CallLowering.h>
#include <Interpreters/GQL/CatalogLowering.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MutationLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB::GQL
{

void lowerPipelinePositionClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (tryLowerCatalogClause(plan, clause, context, environment, scope))
        return;

    if (tryLowerDataModifyingClause(plan, clause, context, environment, scope))
        return;

    if (tryLowerPipelineCallClause(plan, clause, context, environment, scope))
        return;

    lowerPipelineClause(plan, clause, context, scope);
}

}
