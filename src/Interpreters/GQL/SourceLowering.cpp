#include <Interpreters/GQL/SourceLowering.h>

#include <Interpreters/GQL/MatchLowering.h>
#include <Parsers/graph/GraphAST.h>

namespace DB::GQL
{

bool SourceClauseBuffer::tryAppend(const ASTPtr & clause)
{
    if (const auto * match = clause->as<OPENGQL::AST::GQLMatchClause>())
    {
        match_clauses.push_back(match);
        return true;
    }

    return false;
}

bool SourceClauseBuffer::hasPending() const
{
    return !match_clauses.empty();
}

void SourceClauseBuffer::flush(QueryPlan & plan, ContextPtr context, PlanScope & scope)
{
    if (match_clauses.empty())
        return;

    lowerMatchClauseSequence(plan, match_clauses, context, scope);
    match_clauses.clear();
}

}
