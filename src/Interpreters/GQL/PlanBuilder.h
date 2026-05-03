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

class PlanBuilder final
{
public:
    explicit PlanBuilder(ContextPtr context_);

    void buildSingleQuery(QueryPlan & plan, const OPENGQL::AST::GQLSingleQuery & query);

private:
    void lowerMatchSequence(QueryPlan & plan, const std::vector<const OPENGQL::AST::GQLMatchClause *> & matches) const;

    ContextPtr context;
};

}
