#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/graph/fwd_decl.h>

#include <vector>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;
struct PlanEnvironment;

void lowerMatchClauseSequence(
    QueryPlan & plan,
    const std::vector<const OPENGQL::AST::GQLMatchClause *> & matches,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
