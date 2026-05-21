#include <Analyzer/GQL/GQLQueryTreeNodes.h>

#include <Common/assert_cast.h>
#include <Common/SipHash.h>

#include <IO/Operators.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteHelpers.h>

namespace DB
{

namespace ErrorCodes
{
extern const int UNSUPPORTED_METHOD;
}

// ============================================================================
// GQLLinearQueryNode
// ============================================================================

GQLLinearQueryNode::GQLLinearQueryNode()
    : IQueryTreeNode(children_size)
{
    children[steps_child_index] = std::make_shared<ListNode>();
}

void GQLLinearQueryNode::dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const
{
    buffer << std::string(indent, ' ') << "GQL_LINEAR_QUERY id: " << format_state.getNodeId(this);

    if (hasAlias())
        buffer << ", alias: " << getAlias();

    buffer << '\n' << std::string(indent + 2, ' ') << "STEPS\n";
    getStepsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLLinearQueryNode::isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const
{
    return true;
}

void GQLLinearQueryNode::updateTreeHashImpl(HashState &, CompareOptions) const
{
}

QueryTreeNodePtr GQLLinearQueryNode::cloneImpl() const
{
    return std::make_shared<GQLLinearQueryNode>();
}

ASTPtr GQLLinearQueryNode::toASTImpl(const ConvertToASTOptions &) const
{
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLLinearQueryNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLMatchNode
// ============================================================================

GQLMatchNode::GQLMatchNode()
    : IQueryTreeNode(children_size)
{
    children[path_patterns_child_index] = std::make_shared<ListNode>();
}

void GQLMatchNode::dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const
{
    buffer << std::string(indent, ' ') << "GQL_MATCH id: " << format_state.getNodeId(this);

    if (hasAlias())
        buffer << ", alias: " << getAlias();

    if (optional)
        buffer << ", optional: true";

    if (match_mode != OPENGQL::AST::GraphMatchMode::None)
    {
        buffer << ", match_mode: ";
        switch (match_mode)
        {
            case OPENGQL::AST::GraphMatchMode::RepeatableElements:
                buffer << "REPEATABLE_ELEMENTS";
                break;
            case OPENGQL::AST::GraphMatchMode::RepeatableElementBindings:
                buffer << "REPEATABLE_ELEMENT_BINDINGS";
                break;
            case OPENGQL::AST::GraphMatchMode::DifferentEdges:
                buffer << "DIFFERENT_EDGES";
                break;
            case OPENGQL::AST::GraphMatchMode::DifferentEdgeBindings:
                buffer << "DIFFERENT_EDGE_BINDINGS";
                break;
            case OPENGQL::AST::GraphMatchMode::None:
                break;
        }
    }

    buffer << '\n' << std::string(indent + 2, ' ') << "PATH_PATTERNS\n";
    getPathPatternsNode()->dumpTreeImpl(buffer, format_state, indent + 4);

    if (children[where_child_index])
    {
        buffer << '\n' << std::string(indent + 2, ' ') << "WHERE\n";
        children[where_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
    }

    if (children[keep_child_index])
    {
        buffer << '\n' << std::string(indent + 2, ' ') << "KEEP\n";
        children[keep_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
    }

    if (children[yield_child_index])
    {
        buffer << '\n' << std::string(indent + 2, ' ') << "YIELD\n";
        children[yield_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
    }
}

bool GQLMatchNode::isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const
{
    const auto & rhs_typed = assert_cast<const GQLMatchNode &>(rhs);
    return optional == rhs_typed.optional && match_mode == rhs_typed.match_mode;
}

void GQLMatchNode::updateTreeHashImpl(HashState & state, CompareOptions) const
{
    state.update(optional);
    state.update(static_cast<size_t>(match_mode));
}

QueryTreeNodePtr GQLMatchNode::cloneImpl() const
{
    auto result = std::make_shared<GQLMatchNode>();
    result->optional = optional;
    result->match_mode = match_mode;
    return result;
}

ASTPtr GQLMatchNode::toASTImpl(const ConvertToASTOptions &) const
{
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLMatchNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPathPatternNode
// ============================================================================

GQLPathPatternNode::GQLPathPatternNode()
    : IQueryTreeNode(children_size)
{
}

void GQLPathPatternNode::dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const
{
    buffer << std::string(indent, ' ') << "GQL_PATH_PATTERN id: " << format_state.getNodeId(this);

    if (hasAlias())
        buffer << ", alias: " << getAlias();

    if (!path_variable.empty())
        buffer << ", path_variable: " << path_variable;

    if (!prefix.empty())
        buffer << ", prefix: " << prefix;

    if (children[expression_child_index])
    {
        buffer << '\n' << std::string(indent + 2, ' ') << "EXPRESSION\n";
        children[expression_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
    }
}

bool GQLPathPatternNode::isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const
{
    const auto & rhs_typed = assert_cast<const GQLPathPatternNode &>(rhs);
    return path_variable == rhs_typed.path_variable && prefix == rhs_typed.prefix;
}

void GQLPathPatternNode::updateTreeHashImpl(HashState & state, CompareOptions) const
{
    state.update(path_variable.size());
    state.update(path_variable);
    state.update(prefix.size());
    state.update(prefix);
}

QueryTreeNodePtr GQLPathPatternNode::cloneImpl() const
{
    auto result = std::make_shared<GQLPathPatternNode>();
    result->path_variable = path_variable;
    result->prefix = prefix;
    return result;
}

ASTPtr GQLPathPatternNode::toASTImpl(const ConvertToASTOptions &) const
{
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPathPatternNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLCombinedQueryNode
// ============================================================================

GQLCombinedQueryNode::GQLCombinedQueryNode()
    : IQueryTreeNode(children_size)
{
    children[queries_child_index] = std::make_shared<ListNode>();
}

void GQLCombinedQueryNode::dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const
{
    buffer << std::string(indent, ' ') << "GQL_COMBINED_QUERY id: " << format_state.getNodeId(this);

    if (hasAlias())
        buffer << ", alias: " << getAlias();

    buffer << ", operators: [";
    for (size_t i = 0; i < operators.size(); ++i)
    {
        if (i > 0)
            buffer << ", ";
        switch (operators[i])
        {
            case CombinedOperator::UNION_ALL:
                buffer << "UNION_ALL";
                break;
            case CombinedOperator::UNION_DISTINCT:
                buffer << "UNION_DISTINCT";
                break;
            case CombinedOperator::EXCEPT_ALL:
                buffer << "EXCEPT_ALL";
                break;
            case CombinedOperator::EXCEPT_DISTINCT:
                buffer << "EXCEPT_DISTINCT";
                break;
            case CombinedOperator::INTERSECT_ALL:
                buffer << "INTERSECT_ALL";
                break;
            case CombinedOperator::INTERSECT_DISTINCT:
                buffer << "INTERSECT_DISTINCT";
                break;
        }
    }
    buffer << "]";

    buffer << '\n' << std::string(indent + 2, ' ') << "QUERIES\n";
    getQueriesNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLCombinedQueryNode::isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const
{
    const auto & rhs_typed = assert_cast<const GQLCombinedQueryNode &>(rhs);
    return operators == rhs_typed.operators;
}

void GQLCombinedQueryNode::updateTreeHashImpl(HashState & state, CompareOptions) const
{
    state.update(operators.size());
    for (auto op : operators)
        state.update(static_cast<size_t>(op));
}

QueryTreeNodePtr GQLCombinedQueryNode::cloneImpl() const
{
    auto result = std::make_shared<GQLCombinedQueryNode>();
    result->operators = operators;
    return result;
}

ASTPtr GQLCombinedQueryNode::toASTImpl(const ConvertToASTOptions &) const
{
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLCombinedQueryNode::toASTImpl is not implemented yet");
}

}
