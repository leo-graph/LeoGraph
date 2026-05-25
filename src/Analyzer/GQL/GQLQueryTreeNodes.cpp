#include <Analyzer/GQL/GQLQueryTreeNodes.h>

#include <Common/assert_cast.h>
#include <Common/SipHash.h>

#include <IO/Operators.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteHelpers.h>

namespace DB {

namespace ErrorCodes {
extern const int UNSUPPORTED_METHOD;
}

// ============================================================================
// GQLLinearQueryNode
// ============================================================================

GQLLinearQueryNode::GQLLinearQueryNode() : IQueryTreeNode(children_size) { children[steps_child_index] = std::make_shared<ListNode>(); }

void GQLLinearQueryNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_LINEAR_QUERY id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "STEPS\n";
  getStepsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLLinearQueryNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLLinearQueryNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLLinearQueryNode::cloneImpl() const { return std::make_shared<GQLLinearQueryNode>(); }

ASTPtr GQLLinearQueryNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLLinearQueryNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLMatchNode
// ============================================================================

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

// ============================================================================
// GQLPathPatternNode
// ============================================================================

GQLPathPatternNode::GQLPathPatternNode() : IQueryTreeNode(children_size) {}

void GQLPathPatternNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PATH_PATTERN id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (!path_variable.empty()) buffer << ", path_variable: " << path_variable;

  if (!prefix.empty()) buffer << ", prefix: " << prefix;

  if (children[expression_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "EXPRESSION\n";
    children[expression_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLPathPatternNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLPathPatternNode &>(rhs);
  return path_variable == rhs_typed.path_variable && prefix == rhs_typed.prefix;
}

void GQLPathPatternNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(path_variable.size());
  state.update(path_variable);
  state.update(prefix.size());
  state.update(prefix);
}

QueryTreeNodePtr GQLPathPatternNode::cloneImpl() const {
  auto result = std::make_shared<GQLPathPatternNode>();
  result->path_variable = path_variable;
  result->prefix = prefix;
  return result;
}

ASTPtr GQLPathPatternNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPathPatternNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPathTermNode
// ============================================================================

GQLPathTermNode::GQLPathTermNode() : IQueryTreeNode(children_size) { children[elements_child_index] = std::make_shared<ListNode>(); }

void GQLPathTermNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PATH_TERM id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "ELEMENTS\n";
  getElementsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLPathTermNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLPathTermNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLPathTermNode::cloneImpl() const { return std::make_shared<GQLPathTermNode>(); }

ASTPtr GQLPathTermNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPathTermNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPathAlternationNode
// ============================================================================

GQLPathAlternationNode::GQLPathAlternationNode() : IQueryTreeNode(children_size) {
  children[alternatives_child_index] = std::make_shared<ListNode>();
}

void GQLPathAlternationNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PATH_ALTERNATION id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "ALTERNATIVES\n";
  getAlternativesNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLPathAlternationNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLPathAlternationNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLPathAlternationNode::cloneImpl() const { return std::make_shared<GQLPathAlternationNode>(); }

ASTPtr GQLPathAlternationNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPathAlternationNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLSimplifiedPathPatternNode
// ============================================================================

GQLSimplifiedPathPatternNode::GQLSimplifiedPathPatternNode() : IQueryTreeNode(children_size) {}

void GQLSimplifiedPathPatternNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_SIMPLIFIED_PATH_PATTERN id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[expression_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "EXPRESSION\n";
    children[expression_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLSimplifiedPathPatternNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLSimplifiedPathPatternNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLSimplifiedPathPatternNode::cloneImpl() const { return std::make_shared<GQLSimplifiedPathPatternNode>(); }

ASTPtr GQLSimplifiedPathPatternNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLSimplifiedPathPatternNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLSimplifiedPathExprNode
// ============================================================================

GQLSimplifiedPathExprNode::GQLSimplifiedPathExprNode() : IQueryTreeNode(children_size) {}

void GQLSimplifiedPathExprNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_SIMPLIFIED_PATH_EXPR id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[edge_pattern_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "EDGE_PATTERN\n";
    children[edge_pattern_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLSimplifiedPathExprNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLSimplifiedPathExprNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLSimplifiedPathExprNode::cloneImpl() const { return std::make_shared<GQLSimplifiedPathExprNode>(); }

ASTPtr GQLSimplifiedPathExprNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLSimplifiedPathExprNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLOptionalBlockNode
// ============================================================================

GQLOptionalBlockNode::GQLOptionalBlockNode() : IQueryTreeNode(children_size) {}

void GQLOptionalBlockNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_OPTIONAL_BLOCK id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[inner_query_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "INNER_QUERY\n";
    children[inner_query_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLOptionalBlockNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLOptionalBlockNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLOptionalBlockNode::cloneImpl() const { return std::make_shared<GQLOptionalBlockNode>(); }

ASTPtr GQLOptionalBlockNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLOptionalBlockNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLOrderByNode
// ============================================================================

GQLOrderByNode::GQLOrderByNode() : IQueryTreeNode(children_size) { children[sort_items_child_index] = std::make_shared<ListNode>(); }

void GQLOrderByNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_ORDER_BY id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "SORT_ITEMS\n";
  getSortItemsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLOrderByNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLOrderByNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLOrderByNode::cloneImpl() const { return std::make_shared<GQLOrderByNode>(); }

ASTPtr GQLOrderByNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLOrderByNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPageNode
// ============================================================================

GQLPageNode::GQLPageNode() : IQueryTreeNode(children_size) {}

void GQLPageNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PAGE id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[offset_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "OFFSET\n";
    children[offset_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[limit_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "LIMIT\n";
    children[limit_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLPageNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLPageNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLPageNode::cloneImpl() const { return std::make_shared<GQLPageNode>(); }

ASTPtr GQLPageNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPageNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLNodePatternNode
// ============================================================================

GQLNodePatternNode::GQLNodePatternNode() : IQueryTreeNode(children_size) {}

void GQLNodePatternNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_NODE_PATTERN id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (!element_variable.empty()) buffer << ", element_variable: " << element_variable;

  if (children[label_expression_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "LABEL_EXPRESSION\n";
    children[label_expression_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[property_map_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "PROPERTY_MAP\n";
    children[property_map_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[where_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "WHERE\n";
    children[where_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLNodePatternNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLNodePatternNode &>(rhs);
  return element_variable == rhs_typed.element_variable;
}

void GQLNodePatternNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(element_variable.size());
  state.update(element_variable);
}

QueryTreeNodePtr GQLNodePatternNode::cloneImpl() const {
  auto result = std::make_shared<GQLNodePatternNode>();
  result->element_variable = element_variable;
  return result;
}

ASTPtr GQLNodePatternNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLNodePatternNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLEdgePatternNode
// ============================================================================

GQLEdgePatternNode::GQLEdgePatternNode() : IQueryTreeNode(children_size) {}

void GQLEdgePatternNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_EDGE_PATTERN id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (!element_variable.empty()) buffer << ", element_variable: " << element_variable;

  buffer << ", direction: ";
  switch (direction) {
    case Direction::Left:
      buffer << "Left";
      break;
    case Direction::Right:
      buffer << "Right";
      break;
    case Direction::Undirected:
      buffer << "Undirected";
      break;
    case Direction::LeftOrRight:
      buffer << "LeftOrRight";
      break;
    case Direction::LeftOrUndirected:
      buffer << "LeftOrUndirected";
      break;
    case Direction::UndirectedOrRight:
      buffer << "UndirectedOrRight";
      break;
    case Direction::Any:
      buffer << "Any";
      break;
  }

  if (children[label_expression_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "LABEL_EXPRESSION\n";
    children[label_expression_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[quantifier_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "QUANTIFIER\n";
    children[quantifier_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[property_map_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "PROPERTY_MAP\n";
    children[property_map_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[where_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "WHERE\n";
    children[where_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLEdgePatternNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLEdgePatternNode &>(rhs);
  return element_variable == rhs_typed.element_variable && direction == rhs_typed.direction;
}

void GQLEdgePatternNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(element_variable.size());
  state.update(element_variable);
  state.update(static_cast<size_t>(direction));
}

QueryTreeNodePtr GQLEdgePatternNode::cloneImpl() const {
  auto result = std::make_shared<GQLEdgePatternNode>();
  result->element_variable = element_variable;
  result->direction = direction;
  return result;
}

ASTPtr GQLEdgePatternNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLEdgePatternNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLQuantifierNode
// ============================================================================

GQLQuantifierNode::GQLQuantifierNode() : IQueryTreeNode(children_size) {}

void GQLQuantifierNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_QUANTIFIER id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[lower_bound_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "LOWER_BOUND\n";
    children[lower_bound_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }

  if (children[upper_bound_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "UPPER_BOUND\n";
    children[upper_bound_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLQuantifierNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLQuantifierNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLQuantifierNode::cloneImpl() const { return std::make_shared<GQLQuantifierNode>(); }

ASTPtr GQLQuantifierNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLQuantifierNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLLabelExpressionNode
// ============================================================================

GQLLabelExpressionNode::GQLLabelExpressionNode() : IQueryTreeNode(children_size) {
  children[operands_child_index] = std::make_shared<ListNode>();
}

void GQLLabelExpressionNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_LABEL_EXPRESSION id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << ", operator: ";
  switch (op) {
    case Operator::Label:
      buffer << "Label";
      break;
    case Operator::Or:
      buffer << "Or";
      break;
    case Operator::And:
      buffer << "And";
      break;
    case Operator::Not:
      buffer << "Not";
      break;
  }

  if (!label_name.empty()) buffer << ", label_name: " << label_name;

  if (getOperands().getNodes().size() > 0) {
    buffer << '\n' << std::string(indent + 2, ' ') << "OPERANDS\n";
    getOperandsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLLabelExpressionNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLLabelExpressionNode &>(rhs);
  return op == rhs_typed.op && label_name == rhs_typed.label_name;
}

void GQLLabelExpressionNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(static_cast<size_t>(op));
  state.update(label_name.size());
  state.update(label_name);
}

QueryTreeNodePtr GQLLabelExpressionNode::cloneImpl() const {
  auto result = std::make_shared<GQLLabelExpressionNode>();
  result->op = op;
  result->label_name = label_name;
  return result;
}

ASTPtr GQLLabelExpressionNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLLabelExpressionNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPropertyMapNode
// ============================================================================

GQLPropertyMapNode::GQLPropertyMapNode() : IQueryTreeNode(children_size) { children[items_child_index] = std::make_shared<ListNode>(); }

void GQLPropertyMapNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PROPERTY_MAP id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "ITEMS\n";
  getItemsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLPropertyMapNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLPropertyMapNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLPropertyMapNode::cloneImpl() const { return std::make_shared<GQLPropertyMapNode>(); }

ASTPtr GQLPropertyMapNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPropertyMapNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLPropertyItemNode
// ============================================================================

GQLPropertyItemNode::GQLPropertyItemNode() : IQueryTreeNode(children_size) {}

void GQLPropertyItemNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_PROPERTY_ITEM id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (!property_name.empty()) buffer << ", property_name: " << property_name;

  if (children[value_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "VALUE\n";
    children[value_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLPropertyItemNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLPropertyItemNode &>(rhs);
  return property_name == rhs_typed.property_name;
}

void GQLPropertyItemNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(property_name.size());
  state.update(property_name);
}

QueryTreeNodePtr GQLPropertyItemNode::cloneImpl() const {
  auto result = std::make_shared<GQLPropertyItemNode>();
  result->property_name = property_name;
  return result;
}

ASTPtr GQLPropertyItemNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLPropertyItemNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLKeepNode
// ============================================================================

GQLKeepNode::GQLKeepNode() : IQueryTreeNode(children_size) { children[items_child_index] = std::make_shared<ListNode>(); }

void GQLKeepNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_KEEP id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "ITEMS\n";
  getItemsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLKeepNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLKeepNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLKeepNode::cloneImpl() const { return std::make_shared<GQLKeepNode>(); }

ASTPtr GQLKeepNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLKeepNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLYieldNode
// ============================================================================

GQLYieldNode::GQLYieldNode() : IQueryTreeNode(children_size) { children[items_child_index] = std::make_shared<ListNode>(); }

void GQLYieldNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_YIELD id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << '\n' << std::string(indent + 2, ' ') << "ITEMS\n";
  getItemsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLYieldNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLYieldNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLYieldNode::cloneImpl() const { return std::make_shared<GQLYieldNode>(); }

ASTPtr GQLYieldNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLYieldNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLReturnNode
// ============================================================================

GQLReturnNode::GQLReturnNode() : IQueryTreeNode(children_size) { children[items_child_index] = std::make_shared<ListNode>(); }

void GQLReturnNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_RETURN id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (distinct) buffer << ", distinct: true";

  buffer << '\n' << std::string(indent + 2, ' ') << "ITEMS\n";
  getItemsNode()->dumpTreeImpl(buffer, format_state, indent + 4);
}

bool GQLReturnNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLReturnNode &>(rhs);
  return distinct == rhs_typed.distinct;
}

void GQLReturnNode::updateTreeHashImpl(HashState &state, CompareOptions) const { state.update(distinct); }

QueryTreeNodePtr GQLReturnNode::cloneImpl() const {
  auto result = std::make_shared<GQLReturnNode>();
  result->distinct = distinct;
  return result;
}

ASTPtr GQLReturnNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLReturnNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLFilterNode
// ============================================================================

GQLFilterNode::GQLFilterNode() : IQueryTreeNode(children_size) {}

void GQLFilterNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_FILTER id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  if (children[predicate_child_index]) {
    buffer << '\n' << std::string(indent + 2, ' ') << "PREDICATE\n";
    children[predicate_child_index]->dumpTreeImpl(buffer, format_state, indent + 4);
  }
}

bool GQLFilterNode::isEqualImpl([[maybe_unused]] const IQueryTreeNode &rhs, CompareOptions) const { return true; }

void GQLFilterNode::updateTreeHashImpl(HashState &, CompareOptions) const {}

QueryTreeNodePtr GQLFilterNode::cloneImpl() const { return std::make_shared<GQLFilterNode>(); }

ASTPtr GQLFilterNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLFilterNode::toASTImpl is not implemented yet");
}

// ============================================================================
// GQLCombinedQueryNode
// ============================================================================

GQLCombinedQueryNode::GQLCombinedQueryNode() : IQueryTreeNode(children_size) {
  children[queries_child_index] = std::make_shared<ListNode>();
}

void GQLCombinedQueryNode::dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const {
  buffer << std::string(indent, ' ') << "GQL_COMBINED_QUERY id: " << format_state.getNodeId(this);

  if (hasAlias()) buffer << ", alias: " << getAlias();

  buffer << ", operators: [";
  for (size_t i = 0; i < operators.size(); ++i) {
    if (i > 0) buffer << ", ";
    switch (operators[i]) {
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

bool GQLCombinedQueryNode::isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const {
  const auto &rhs_typed = assert_cast<const GQLCombinedQueryNode &>(rhs);
  return operators == rhs_typed.operators;
}

void GQLCombinedQueryNode::updateTreeHashImpl(HashState &state, CompareOptions) const {
  state.update(operators.size());
  for (auto op : operators) state.update(static_cast<size_t>(op));
}

QueryTreeNodePtr GQLCombinedQueryNode::cloneImpl() const {
  auto result = std::make_shared<GQLCombinedQueryNode>();
  result->operators = operators;
  return result;
}

ASTPtr GQLCombinedQueryNode::toASTImpl(const ConvertToASTOptions &) const {
  throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "GQLCombinedQueryNode::toASTImpl is not implemented yet");
}

}  // namespace DB
