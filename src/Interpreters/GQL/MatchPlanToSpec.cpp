#include <Interpreters/GQL/MatchPlanToSpec.h>

#include <Parsers/IAST.h>
#include <Parsers/graph/AST/GQLExpr.h>
#include <Parsers/graph/AST/GQLLabelExpression.h>
#include <Parsers/graph/AST/GQLPropertyMap.h>
#include <Parsers/graph/AST/GQLQuantifier.h>

namespace DB::GQL
{
namespace
{

ASTPtr cloneOrNull(const IAST * ast)
{
    return ast ? ast->clone() : nullptr;
}

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
        case EdgeBinding::Direction::IncomingOrOutgoing:
            return Graph::MatchEdgeDirection::IncomingOrOutgoing;
        case EdgeBinding::Direction::IncomingOrUndirected:
            return Graph::MatchEdgeDirection::IncomingOrUndirected;
        case EdgeBinding::Direction::UndirectedOrOutgoing:
            return Graph::MatchEdgeDirection::UndirectedOrOutgoing;
        case EdgeBinding::Direction::Any:
            return Graph::MatchEdgeDirection::Any;
    }

    return Graph::MatchEdgeDirection::Any;
}

Graph::MatchNodeSpec makeNodeSpec(const NodeBinding & node)
{
    return Graph::MatchNodeSpec{
        .variable = node.variable,
        .label_expression = cloneOrNull(node.label),
        .properties = cloneOrNull(node.properties),
        .predicate = cloneOrNull(node.where),
    };
}

Graph::MatchEdgeSpec makeEdgeSpec(const EdgeBinding & edge)
{
    return Graph::MatchEdgeSpec{
        .variable = edge.variable,
        .direction = makeEdgeDirection(edge.direction),
        .label_expression = cloneOrNull(edge.label),
        .properties = cloneOrNull(edge.properties),
        .predicate = cloneOrNull(edge.where),
        .quantifier = cloneOrNull(edge.quantifier),
    };
}

Graph::MatchPathSpec makePathSpec(const PathBinding & path)
{
    Graph::MatchPathSpec result;
    result.variable = path.variable;
    result.prefix = cloneOrNull(path.prefix);
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
