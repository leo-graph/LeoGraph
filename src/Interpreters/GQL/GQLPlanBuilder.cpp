#include <Interpreters/GQL/GQLPlanBuilder.h>

#include <Interpreters/GQL/ClauseSequencePlanner.h>
#include <utility>

#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB
{

namespace GQL
{
namespace GAST = DB::OPENGQL::AST;

GQLPlanBuilder::GQLPlanBuilder(ContextPtr context_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
{
}

GQLPlanBuilder::GQLPlanBuilder(ContextPtr context_, PlanScope scope_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
    , scope(std::move(scope_))
{
}

void GQLPlanBuilder::buildSingleQuery(QueryPlan & plan, const GAST::GQLSingleQuery & query)
{
    planClauseSequence(plan, query.clauses, context, environment, scope);
}

}

}
