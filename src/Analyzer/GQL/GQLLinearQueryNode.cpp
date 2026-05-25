#include <Analyzer/GQL/GQLLinearQueryNode.h>

#include <Common/SipHash.h>
#include <IO/WriteBuffer.h>
#include <IO/Operators.h>

namespace DB
{

namespace ErrorCodes
{
extern const int UNSUPPORTED_METHOD;
}

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

bool GQLLinearQueryNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode & rhs, CompareOptions) const
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

}
