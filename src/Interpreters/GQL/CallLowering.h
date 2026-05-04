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

void lowerInlineCallSource(
    QueryPlan & plan,
    const OPENGQL::AST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

void lowerInlineCallPipelineClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
