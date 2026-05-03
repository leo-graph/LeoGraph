#pragma once

#include <Interpreters/IInterpreter.h>
#include <Parsers/IAST_fwd.h>

#include <memory>

namespace DB
{

class QueryPlan;

namespace Graph
{
class IMatchSourceFactory;
using MatchSourceFactoryPtr = std::shared_ptr<const IMatchSourceFactory>;
}

class InterpreterGQLQuery final : public IInterpreter, WithContext
{
public:
    InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_, Graph::MatchSourceFactoryPtr match_source_factory_ = {});

    BlockIO execute() override;

    void buildQueryPlan(QueryPlan & query_plan);

private:
    ASTPtr query_ptr;
    Graph::MatchSourceFactoryPtr match_source_factory;
};

}
