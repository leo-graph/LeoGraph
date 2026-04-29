#pragma once

#include <Interpreters/IInterpreter.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{

class QueryPlan;

class InterpreterGQLQuery final : public IInterpreter, WithContext
{
public:
    InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_);

    BlockIO execute() override;

    void buildQueryPlan(QueryPlan & query_plan);

private:
    void executeMatch(QueryPlan & query_plan);
    void executeWhere(QueryPlan & query_plan);
    void executeReturn(QueryPlan & query_plan);
    void executePage(QueryPlan & query_plan);

    ASTPtr query_ptr;
};

}
