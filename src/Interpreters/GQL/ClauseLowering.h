#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

#include <string_view>

namespace DB
{
class QueryPlan;
}

namespace DB::OPENGQL::AST
{
class GQLPageClause;
class GQLReturnClause;
class GQLWhereClause;
}

namespace DB::GQL
{

/// Lowers reusable GQL clauses that transform an existing query pipeline.
void lowerWhereClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLWhereClause & where,
    ContextPtr context,
    std::string_view context_name);

void lowerReturnClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLReturnClause & ret,
    ContextPtr context);

void lowerProjectionItems(
    QueryPlan & plan,
    const ASTs & items,
    ContextPtr context,
    std::string_view context_name);

void lowerPageClause(QueryPlan & plan, const OPENGQL::AST::GQLPageClause & page);

void lowerPipelineClause(QueryPlan & plan, const ASTPtr & clause_ast, ContextPtr context);

}
