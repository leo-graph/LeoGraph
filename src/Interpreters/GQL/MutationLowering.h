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

bool isDataModifyingClause(const ASTPtr & clause);

void lowerDataModifyingClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    PlanScope & scope);

}
