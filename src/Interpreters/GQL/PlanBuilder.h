#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/graph/fwd_decl.h>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanBuilder final
{
public:
    explicit PlanBuilder(ContextPtr context_);

    void buildSingleQuery(QueryPlan & plan, const OPENGQL::AST::GQLSingleQuery & query);

private:
    ContextPtr context;
};

}
