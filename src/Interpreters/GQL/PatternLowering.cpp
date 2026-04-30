#include <Interpreters/GQL/PatternLowering.h>

#include <Common/Exception.h>
#include <Parsers/graph/AST/GQLEdgePattern.h>
#include <Parsers/graph/AST/GQLExpr.h>
#include <Parsers/graph/AST/GQLLabelExpression.h>
#include <Parsers/graph/AST/GQLNodePattern.h>
#include <Parsers/graph/AST/GQLPathPattern.h>
#include <Parsers/graph/AST/GQLPathPatternAlternation.h>
#include <Parsers/graph/AST/GQLPathTerm.h>
#include <Parsers/graph/AST/GQLPropertyMap.h>

namespace DB::ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

using GQLEdgePattern = OPENGQL::AST::GQLEdgePattern;
using GQLExpr = OPENGQL::AST::GQLExpr;
using GQLLabelExpression = OPENGQL::AST::GQLLabelExpression;
using GQLNodePattern = OPENGQL::AST::GQLNodePattern;
using GQLPathPattern = OPENGQL::AST::GQLPathPattern;
using GQLPathPatternAlternation = OPENGQL::AST::GQLPathPatternAlternation;
using GQLPathTerm = OPENGQL::AST::GQLPathTerm;
using GQLPropertyMap = OPENGQL::AST::GQLPropertyMap;

[[noreturn]] void throwUnsupportedPathPattern(const String & reason)
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL path pattern: {}", reason);
}

String astID(const IAST & ast)
{
    return ast.getID(' ');
}

String edgeDirectionToString(OPENGQL::AST::EdgeDirection direction)
{
    switch (direction)
    {
        case OPENGQL::AST::EdgeDirection::Left:
            return "Left";
        case OPENGQL::AST::EdgeDirection::Right:
            return "Right";
        case OPENGQL::AST::EdgeDirection::Undirected:
            return "Undirected";
        case OPENGQL::AST::EdgeDirection::LeftOrRight:
            return "LeftOrRight";
        case OPENGQL::AST::EdgeDirection::LeftOrUndirected:
            return "LeftOrUndirected";
        case OPENGQL::AST::EdgeDirection::UndirectedOrRight:
            return "UndirectedOrRight";
        case OPENGQL::AST::EdgeDirection::Any:
            return "Any";
    }

    return "Unknown";
}

String identifierVariable(const OPENGQL::AST::Ptr & variable)
{
    if (!variable)
        return {};

    const auto * expr = variable->as<GQLExpr>();
    if (!expr || expr->kind != GQLExpr::Kind::Identifier)
        return {};

    return expr->text;
}

template <typename T>
const T * getOptionalChild(const OPENGQL::AST::Ptr & child, const char * field_name)
{
    if (!child)
        return nullptr;

    const auto * result = child->as<T>();
    if (!result)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unexpected GQL pattern {} child type: {}", field_name, astID(*child));

    return result;
}

EdgeBinding::Direction lowerEdgeDirection(OPENGQL::AST::EdgeDirection direction)
{
    // note: AST `Right` is outgoing from the preceding node, `Left` is incoming, `Undirected` stays undirected, and `Any` stays unconstrained.
    switch (direction)
    {
        case OPENGQL::AST::EdgeDirection::Right:
            return EdgeBinding::Direction::Outgoing;
        case OPENGQL::AST::EdgeDirection::Left:
            return EdgeBinding::Direction::Incoming;
        case OPENGQL::AST::EdgeDirection::Undirected:
            return EdgeBinding::Direction::Undirected;
        case OPENGQL::AST::EdgeDirection::Any:
            return EdgeBinding::Direction::Any;
        case OPENGQL::AST::EdgeDirection::LeftOrRight:
        case OPENGQL::AST::EdgeDirection::LeftOrUndirected:
        case OPENGQL::AST::EdgeDirection::UndirectedOrRight:
            throwUnsupportedPathPattern(fmt::format("edge direction {} is not representable by EdgeBinding::Direction", edgeDirectionToString(direction)));
    }

    throwUnsupportedPathPattern(fmt::format("edge direction {} is not supported", edgeDirectionToString(direction)));
}

NodeBinding lowerNodePattern(const GQLNodePattern & node)
{
    NodeBinding binding;
    binding.variable = identifierVariable(node.variable);
    binding.label = getOptionalChild<GQLLabelExpression>(node.label_expression, "node label");
    binding.properties = getOptionalChild<GQLPropertyMap>(node.properties, "node properties");
    binding.where = getOptionalChild<GQLExpr>(node.where, "node where");
    return binding;
}

EdgeBinding lowerEdgePattern(const GQLEdgePattern & edge)
{
    if (edge.quantifier)
        throwUnsupportedPathPattern("edge quantifier");

    EdgeBinding binding;
    binding.variable = identifierVariable(edge.variable);
    binding.direction = lowerEdgeDirection(edge.direction);
    binding.label = getOptionalChild<GQLLabelExpression>(edge.label_expression, "edge label");
    binding.properties = getOptionalChild<GQLPropertyMap>(edge.properties, "edge properties");
    binding.where = getOptionalChild<GQLExpr>(edge.where, "edge where");
    return binding;
}

const GQLPathTerm & getOnlyPathTerm(const GQLPathPattern & path)
{
    if (!path.expression)
        throwUnsupportedPathPattern("missing path term");

    if (const auto * term = path.expression->as<GQLPathTerm>())
        return *term;

    if (path.expression->as<GQLPathPatternAlternation>())
        throwUnsupportedPathPattern("multiple GQLPathTerms in path alternation");

    throwUnsupportedPathPattern(fmt::format("path expression {} is not a GQLPathTerm", astID(*path.expression)));
}

}

PathBinding lowerPathPattern(const OPENGQL::AST::GQLPathPattern & path)
{
    if (path.variable)
        throwUnsupportedPathPattern("path variable");

    if (path.prefix)
        throwUnsupportedPathPattern("path prefix");

    const auto & term = getOnlyPathTerm(path);
    if (term.factors.empty())
        throwUnsupportedPathPattern("empty path term");

    if (term.factors.size() % 2 == 0)
        throwUnsupportedPathPattern("path term must contain an odd number of alternating node and edge factors");

    PathBinding result;
    for (size_t i = 0; i < term.factors.size(); ++i)
    {
        const auto & factor = term.factors[i];
        if (!factor)
            throwUnsupportedPathPattern(fmt::format("path factor {} is null", i));

        if (i % 2 == 0)
        {
            const auto * node = factor->as<GQLNodePattern>();
            if (!node)
                throwUnsupportedPathPattern(fmt::format("path factor {} must be a GQLNodePattern, got {}", i, astID(*factor)));

            result.nodes.push_back(lowerNodePattern(*node));
        }
        else
        {
            const auto * edge = factor->as<GQLEdgePattern>();
            if (!edge)
                throwUnsupportedPathPattern(fmt::format("path factor {} must be a GQLEdgePattern, got {}", i, astID(*factor)));

            result.edges.push_back(lowerEdgePattern(*edge));
        }
    }

    if (result.edges.size() + 1 != result.nodes.size())
        throw Exception(
            ErrorCodes::LOGICAL_ERROR,
            "Invalid lowered GQL path binding: {} nodes and {} edges",
            result.nodes.size(),
            result.edges.size());

    return result;
}

}
