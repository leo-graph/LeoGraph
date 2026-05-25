#include <Analyzer/GQL/GQLMatchNode.h>

#include <Common/assert_cast.h>
#include <Common/SipHash.h>
#include <IO/WriteBuffer.h>
#include <IO/Operators.h>

namespace DB
{

namespace ErrorCodes
{
extern const int UNSUPPORTED_METHOD;
}

GQLMatchNode::GQLMatchNode() : IQueryTreeNode(children_size) { children[path_patterns_child_index] = std::make_shared<ListNode>(); }

void GQLMatchNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_MATCH id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (optional) buffer << ", optional: true";

  if (match_mode != OPENGQL::AST::GraphMatchMode::None) {
    buffer << ", match_mode: ";
    switch (match_mode) {
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

  if (children[where_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "WHERE\n";
    children[where_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[keep_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "KEEP\n";
    children[keep_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[yield_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "YIELD\n";
    children[yield_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLMatchNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLMatchNode &>(rhs);
  return optional == rhs_typed.optional && match_mode == rhs_typed.match_mode;
}

void GQLMatchNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(optional);
  state.update(static_cast<size_t>(match_mode));
}

QueryTreeNodePtr GQLMatchNode::cloneImpl() const {
  auto result = std::make_shared<GQLMatchNode>();
  result->optional = optional;
  result->match_mode = match_mode;
  return result;
}

ASTPtr GQLMatchNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLMatchNode::toASTImpl is not implemented yet");
}

}
