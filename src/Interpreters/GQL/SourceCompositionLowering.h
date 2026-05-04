#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/graph/fwd_decl.h>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;
struct PlanEnvironment;

void lowerSelectSourceList(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSelectSourceList & source_list,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
