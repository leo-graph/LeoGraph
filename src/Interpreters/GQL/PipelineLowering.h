#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

struct PlanEnvironment;
class PlanScope;

/// Lowers a clause that appears after a source pipeline already exists.
///
/// This dispatch layer keeps service-aware boundaries such as catalog, DML, and
/// inline `CALL` close to `PlanEnvironment`, while `ClauseLowering` remains a
/// pure pipeline-transform helper.
void lowerPipelinePositionClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
