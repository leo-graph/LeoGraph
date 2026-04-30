#pragma once

#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Parsers/graph/AST/GQLExpr.h>

namespace DB::GQL
{

const ActionsDAG::Node & lowerExpression(
    const OPENGQL::AST::GQLExpr & expr,
    ActionsDAG & dag,
    ContextPtr context);

}
