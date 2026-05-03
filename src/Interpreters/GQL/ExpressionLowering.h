#pragma once

#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Parsers/graph/AST/GQLExpr.h>

namespace DB::GQL
{

class PlanScope;

const ActionsDAG::Node & lowerExpression(
    const OPENGQL::AST::GQLExpr & expr,
    ActionsDAG & dag,
    ContextPtr context);

const ActionsDAG::Node & lowerExpression(
    const OPENGQL::AST::GQLExpr & expr,
    ActionsDAG & dag,
    ContextPtr context,
    const PlanScope & scope);

}
