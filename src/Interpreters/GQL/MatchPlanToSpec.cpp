#include <Interpreters/GQL/MatchPlanToSpec.h>

namespace DB::GQL
{
namespace
{

Graph::MatchEdgeDirection makeEdgeDirection(EdgeBinding::Direction direction)
{
    switch (direction)
    {
        case EdgeBinding::Direction::Outgoing:
            return Graph::MatchEdgeDirection::Outgoing;
        case EdgeBinding::Direction::Incoming:
            return Graph::MatchEdgeDirection::Incoming;
        case EdgeBinding::Direction::Undirected:
            return Graph::MatchEdgeDirection::Undirected;
        case EdgeBinding::Direction::Any:
            return Graph::MatchEdgeDirection::Any;
    }

    return Graph::MatchEdgeDirection::Any;
}

Graph::MatchNodeSpec makeNodeSpec(const NodeBinding & node)
{
    return Graph::MatchNodeSpec{
        .variable = node.variable,
        .has_label_expression = node.label != nullptr,
        .has_properties = node.properties != nullptr,
        .has_predicate = node.where != nullptr,
    };
}

Graph::MatchEdgeSpec makeEdgeSpec(const EdgeBinding & edge)
{
    return Graph::MatchEdgeSpec{
        .variable = edge.variable,
        .direction = makeEdgeDirection(edge.direction),
        .has_label_expression = edge.label != nullptr,
        .has_properties = edge.properties != nullptr,
        .has_predicate = edge.where != nullptr,
    };
}

Graph::MatchPathSpec makePathSpec(const PathBinding & path)
{
    Graph::MatchPathSpec result;
    result.nodes.reserve(path.nodes.size());
    for (const auto & node : path.nodes)
        result.nodes.push_back(makeNodeSpec(node));

    result.edges.reserve(path.edges.size());
    for (const auto & edge : path.edges)
        result.edges.push_back(makeEdgeSpec(edge));

    return result;
}

}

Graph::MatchSpec makeMatchSpec(const MatchPlan & match)
{
    Graph::MatchSpec result;
    result.optional = match.optional;
    result.has_match_mode = match.match_mode != OPENGQL::AST::GraphMatchMode::None;
    result.has_keep_clause = match.has_keep_clause;
    result.has_optional_operand_block = match.has_optional_operand_block;
    result.has_yield_items = !match.yield_items.empty();

    result.paths.reserve(match.paths.size());
    for (const auto & path : match.paths)
        result.paths.push_back(makePathSpec(path));

    return result;
}

}
