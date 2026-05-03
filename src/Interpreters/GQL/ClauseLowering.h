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
class GQLSelectClause;
class GQLLetClause;
class GQLForClause;
class GQLWhereClause;
}

namespace DB::GQL
{

class PlanScope;

/// Lowers reusable GQL clauses that transform an existing query pipeline.
void lowerWhereClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLWhereClause & where,
    ContextPtr context,
    std::string_view context_name,
    const PlanScope & scope);

void lowerReturnClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLReturnClause & ret,
    ContextPtr context,
    PlanScope & scope);

void lowerSelectClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSelectClause & select,
    ContextPtr context,
    PlanScope & scope,
    bool source_was_lowered = false);

void lowerProjectionItems(
    QueryPlan & plan,
    const ASTs & items,
    ContextPtr context,
    std::string_view context_name,
    PlanScope & scope);

void lowerLetClause(QueryPlan & plan, const OPENGQL::AST::GQLLetClause & let, ContextPtr context, PlanScope & scope);

void lowerForClause(QueryPlan & plan, const OPENGQL::AST::GQLForClause & for_clause, ContextPtr context, PlanScope & scope);

void lowerPageClause(QueryPlan & plan, const OPENGQL::AST::GQLPageClause & page, ContextPtr context, PlanScope & scope);

void lowerPipelineClause(QueryPlan & plan, const ASTPtr & clause_ast, ContextPtr context, PlanScope & scope);

}
