#pragma once

#include <Interpreters/Context_fwd.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;

void buildRootQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanEnvironment & environment);

void buildRootQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
