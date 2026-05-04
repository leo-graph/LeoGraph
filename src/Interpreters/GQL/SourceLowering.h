#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>
#include <Parsers/graph/fwd_decl.h>

#include <vector>

namespace DB
{
class QueryPlan;
}

namespace DB::GQL
{

class PlanScope;
struct PlanEnvironment;

void lowerUseClause(const OPENGQL::AST::GQLUseClause & use, PlanScope & scope);

bool isSourceIntroducingClause(const ASTPtr & clause);

class SourceClauseBuffer final
{
public:
    bool tryAppend(const ASTPtr & clause);
    bool hasPending() const;
    void flush(QueryPlan & plan, ContextPtr context, const PlanEnvironment & environment, PlanScope & scope);

private:
    std::vector<const OPENGQL::AST::GQLMatchClause *> match_clauses;
};

bool tryLowerStandaloneSourceClause(
    QueryPlan & plan,
    const ASTPtr & clause,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

}
