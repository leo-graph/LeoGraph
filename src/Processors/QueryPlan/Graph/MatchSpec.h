#pragma once

#include <Parsers/IAST_fwd.h>
#include <base/types.h>

#include <vector>

namespace DB::Graph
{

enum class MatchEdgeDirection : UInt8
{
    Outgoing,
    Incoming,
    Undirected,
    Any,
};

struct MatchNodeSpec
{
    String variable;
    ASTPtr label_expression;
    ASTPtr properties;
    ASTPtr predicate;
};

struct MatchEdgeSpec
{
    String variable;
    MatchEdgeDirection direction = MatchEdgeDirection::Any;
    ASTPtr label_expression;
    ASTPtr properties;
    ASTPtr predicate;
};

struct MatchPathSpec
{
    std::vector<MatchNodeSpec> nodes;
    std::vector<MatchEdgeSpec> edges;
};

struct MatchSpec
{
    bool optional = false;
    bool has_match_mode = false;
    bool has_keep_clause = false;
    bool has_optional_operand_block = false;
    bool has_yield_items = false;
    std::vector<MatchPathSpec> paths;
};

}
