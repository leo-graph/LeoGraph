#pragma once

#include <Parsers/graph/fwd_decl.h>

#include <vector>

namespace DB::GQL
{

struct NodeBinding
{
    String variable;
    const OPENGQL::AST::GQLLabelExpression * label = nullptr;
    const OPENGQL::AST::GQLPropertyMap * properties = nullptr;
    const OPENGQL::AST::GQLExpr * where = nullptr;
};

struct EdgeBinding
{
    enum class Direction : UInt8
    {
        Outgoing,
        Incoming,
        Undirected,
        IncomingOrOutgoing,
        IncomingOrUndirected,
        UndirectedOrOutgoing,
        Any,
    };

    String variable;
    Direction direction = Direction::Any;
    const OPENGQL::AST::GQLLabelExpression * label = nullptr;
    const OPENGQL::AST::GQLPropertyMap * properties = nullptr;
    const OPENGQL::AST::GQLExpr * where = nullptr;
    const OPENGQL::AST::GQLQuantifier * quantifier = nullptr;
};

struct PathBinding
{
    String variable;
    const IAST * prefix = nullptr;
    std::vector<NodeBinding> nodes;
    std::vector<EdgeBinding> edges;
};

struct MatchPlan
{
    bool optional = false;
    OPENGQL::AST::GraphMatchMode match_mode = OPENGQL::AST::GraphMatchMode::None;
    const OPENGQL::AST::GQLKeepClause * keep_clause = nullptr;
    bool has_keep_clause = false;
    bool has_optional_operand_block = false;
    std::vector<PathBinding> paths;
    std::vector<const OPENGQL::AST::GQLExpr *> yield_items;
    const OPENGQL::AST::GQLWhereClause * where = nullptr;
};

PathBinding lowerPathPattern(const OPENGQL::AST::GQLPathPattern & path_pattern);
MatchPlan lowerMatchClause(const OPENGQL::AST::GQLMatchClause & match);

}
