#pragma once

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
    bool has_label_expression = false;
    bool has_properties = false;
    bool has_predicate = false;
};

struct MatchEdgeSpec
{
    String variable;
    MatchEdgeDirection direction = MatchEdgeDirection::Any;
    bool has_label_expression = false;
    bool has_properties = false;
    bool has_predicate = false;
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
