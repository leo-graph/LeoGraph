#include <Interpreters/GQL/PlanBuilder.h>

#include <Interpreters/GQL/ClauseSequenceLowering.h>
#include <utility>

#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB
{

namespace GQL
{
namespace GAST = DB::OPENGQL::AST;

PlanBuilder::PlanBuilder(ContextPtr context_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
{
}

PlanBuilder::PlanBuilder(ContextPtr context_, PlanScope scope_, PlanEnvironment environment_)
    : context(std::move(context_))
    , environment(std::move(environment_))
    , scope(std::move(scope_))
{
}

void PlanBuilder::buildSingleQuery(QueryPlan & plan, const GAST::GQLSingleQuery & query)
{
    lowerClauseSequence(plan, query.clauses, context, environment, scope);
}

}

}
