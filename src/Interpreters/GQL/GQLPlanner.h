#pragma once

#include <Analyzer/IQueryTreeNode.h>
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

// Legacy AST-based interface (will be deprecated)
void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanEnvironment & environment);

void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

// New QueryTree-based interface (preferred)
void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const QueryTreeNodePtr & query_tree,
    ContextPtr context,
    const PlanEnvironment & environment);

void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const QueryTreeNodePtr & query_tree,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
