#pragma once

#include <Interpreters/IInterpreter.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{

class QueryPlan;

class InterpreterGQLQuery final : public IInterpreter, WithContext
{
public:
    InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_, GQL::PlanEnvironment environment_ = {});

    BlockIO execute() override;

    void buildQueryPlan(QueryPlan & query_plan);

private:
    ASTPtr query_ptr;
    GQL::PlanEnvironment environment;
};

}
