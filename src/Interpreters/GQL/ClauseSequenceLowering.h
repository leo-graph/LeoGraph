#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;
struct PlanEnvironment;

/// Lowers a linear `GQLSingleQuery` clause sequence.
///
/// This is the `GQL` counterpart of a clause-query interpreter: it owns clause
/// ordering and source/pipeline boundary decisions, while individual lowering
/// modules keep the implementation for each clause family.
void lowerClauseSequence(
    QueryPlan & plan,
    const ASTs & clauses,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
