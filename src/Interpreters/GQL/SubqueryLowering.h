#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/graph/fwd_decl.h>

#include <string_view>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;
struct PlanEnvironment;

const OPENGQL::AST::GQLSingleQuery & getSingleQuerySubquery(
    const OPENGQL::AST::GQLSubquery & subquery,
    std::string_view context_name);

void lowerSubqueryBindings(
    const OPENGQL::AST::GQLBindingVariableDefinitionBlock & block,
    PlanScope & scope,
    std::string_view context_name);

void lowerSubquerySource(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    PlanScope child_scope,
    std::string_view context_name);

void lowerPipelineOnlySubquery(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    std::string_view context_name);

}
