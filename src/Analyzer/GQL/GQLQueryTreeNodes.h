#pragma once

#include <Analyzer/IQueryTreeNode.h>
#include <Analyzer/ListNode.h>
#include <Parsers/graph/AST/Utils.h>

namespace DB {

namespace OPENGQL::AST {
class GQLSingleQuery;
class GQLCombinedQuery;
class GQLMatchClause;
class GQLPathPattern;
}  // namespace OPENGQL::AST

/** GQL Linear Query Node represents a single GQL query with ordered clause pipeline.
 *
 * Corresponds to parser AST GQLSingleQuery but stores semantic steps rather than raw grammar nodes.
 *
 * Example: MATCH (n) WHERE n.age > 18 RETURN n ORDER BY n.age LIMIT 10
 */
class GQLLinearQueryNode;
using GQLLinearQueryNodePtr = std::shared_ptr<GQLLinearQueryNode>;

class GQLLinearQueryNode final : public IQueryTreeNode {
 public:
  GQLLinearQueryNode();

  /// Get ordered clause steps
  const ListNode &getSteps() const { return children[steps_child_index]->as<const ListNode &>(); }

  /// Get ordered clause steps
  ListNode &getSteps() { return children[steps_child_index]->as<ListNode &>(); }

  /// Get steps node
  const QueryTreeNodePtr &getStepsNode() const { return children[steps_child_index]; }

  /// Get steps node
  QueryTreeNodePtr &getStepsNode() { return children[steps_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_LINEAR_QUERY; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t steps_child_index = 0;
  static constexpr size_t children_size = steps_child_index + 1;
};

/** GQL Match Node represents a MATCH clause in GQL query.
 *
 * Corresponds to parser AST GQLMatchClause.
 *
 * Example: MATCH (n:Person)-[r:KNOWS]->(m:Person) WHERE n.age > 18 YIELD n, r, m
 */
class GQLMatchNode;
using GQLMatchNodePtr = std::shared_ptr<GQLMatchNode>;

class GQLMatchNode final : public IQueryTreeNode {
 public:
  GQLMatchNode();

  /// Returns true if this is OPTIONAL MATCH
  bool isOptional() const { return optional; }

  /// Set optional flag
  void setOptional(bool optional_value) { optional = optional_value; }

  /// Get match mode
  OPENGQL::AST::GraphMatchMode getMatchMode() const { return match_mode; }

  /// Set match mode
  void setMatchMode(OPENGQL::AST::GraphMatchMode match_mode_value) { match_mode = match_mode_value; }

  /// Get path patterns
  const ListNode &getPathPatterns() const { return children[path_patterns_child_index]->as<const ListNode &>(); }

  /// Get path patterns
  ListNode &getPathPatterns() { return children[path_patterns_child_index]->as<ListNode &>(); }

  /// Get path patterns node
  const QueryTreeNodePtr &getPathPatternsNode() const { return children[path_patterns_child_index]; }

  /// Get path patterns node
  QueryTreeNodePtr &getPathPatternsNode() { return children[path_patterns_child_index]; }

  /// Get WHERE predicate
  const QueryTreeNodePtr &getWhere() const { return children[where_child_index]; }

  /// Get WHERE predicate
  QueryTreeNodePtr &getWhere() { return children[where_child_index]; }

  /// Get KEEP clause
  const QueryTreeNodePtr &getKeep() const { return children[keep_child_index]; }

  /// Get KEEP clause
  QueryTreeNodePtr &getKeep() { return children[keep_child_index]; }

  /// Get YIELD items
  const QueryTreeNodePtr &getYield() const { return children[yield_child_index]; }

  /// Get YIELD items
  QueryTreeNodePtr &getYield() { return children[yield_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_MATCH; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  bool optional = false;
  OPENGQL::AST::GraphMatchMode match_mode = OPENGQL::AST::GraphMatchMode::None;

  static constexpr size_t path_patterns_child_index = 0;
  static constexpr size_t where_child_index = 1;
  static constexpr size_t keep_child_index = 2;
  static constexpr size_t yield_child_index = 3;
  static constexpr size_t children_size = yield_child_index + 1;
};

/** GQL Path Pattern Node represents a path pattern in MATCH clause.
 *
 * Corresponds to parser AST GQLPathPattern.
 *
 * Example: p = (a)-[r]->(b)
 * Example: ANY SHORTEST (a)-[r*1..5]->(b)
 */
class GQLPathPatternNode;
using GQLPathPatternNodePtr = std::shared_ptr<GQLPathPatternNode>;

class GQLPathPatternNode final : public IQueryTreeNode {
 public:
  GQLPathPatternNode();

  /// Get path variable name (e.g., 'p' in 'p = (a)-[r]->(b)')
  const String &getPathVariable() const { return path_variable; }

  /// Set path variable name
  void setPathVariable(String path_variable_value) { path_variable = std::move(path_variable_value); }

  /// Get path prefix (e.g., 'ANY SHORTEST')
  const String &getPrefix() const { return prefix; }

  /// Set path prefix
  void setPrefix(String prefix_value) { prefix = std::move(prefix_value); }

  /// Get path expression (GQLPathTermNode | GQLPathAlternationNode | GQLSimplifiedPathPatternNode)
  const QueryTreeNodePtr &getExpression() const { return children[expression_child_index]; }

  /// Get path expression
  QueryTreeNodePtr &getExpression() { return children[expression_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_PATTERN; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  String path_variable;
  String prefix;

  static constexpr size_t expression_child_index = 0;
  static constexpr size_t children_size = expression_child_index + 1;
};

/** GQL Path Term Node represents a classic node/edge chain pattern.
 *
 * Example: (a)-[r]->(b)-[s]->(c)
 */
class GQLPathTermNode;
using GQLPathTermNodePtr = std::shared_ptr<GQLPathTermNode>;

class GQLPathTermNode final : public IQueryTreeNode {
 public:
  GQLPathTermNode();

  /// Get elements (alternating GQLNodePatternNode and GQLEdgePatternNode)
  const ListNode &getElements() const { return children[elements_child_index]->as<const ListNode &>(); }

  /// Get elements
  ListNode &getElements() { return children[elements_child_index]->as<ListNode &>(); }

  /// Get elements node
  const QueryTreeNodePtr &getElementsNode() const { return children[elements_child_index]; }

  /// Get elements node
  QueryTreeNodePtr &getElementsNode() { return children[elements_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_TERM; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t elements_child_index = 0;
  static constexpr size_t children_size = elements_child_index + 1;
};

/** GQL Path Alternation Node represents path alternation (path1 | path2 | ...).
 *
 * Example: (a)-[r]->(b) | (a)-[s]->(c)
 */
class GQLPathAlternationNode;
using GQLPathAlternationNodePtr = std::shared_ptr<GQLPathAlternationNode>;

class GQLPathAlternationNode final : public IQueryTreeNode {
 public:
  GQLPathAlternationNode();

  /// Get alternatives (list of GQLPathTermNode or nested GQLPathAlternationNode)
  const ListNode &getAlternatives() const { return children[alternatives_child_index]->as<const ListNode &>(); }

  /// Get alternatives
  ListNode &getAlternatives() { return children[alternatives_child_index]->as<ListNode &>(); }

  /// Get alternatives node
  const QueryTreeNodePtr &getAlternativesNode() const { return children[alternatives_child_index]; }

  /// Get alternatives node
  QueryTreeNodePtr &getAlternativesNode() { return children[alternatives_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PATH_ALTERNATION; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t alternatives_child_index = 0;
  static constexpr size_t children_size = alternatives_child_index + 1;
};

/** GQL Simplified Path Pattern Node represents simplified path pattern syntax.
 *
 * Example: -[r]->
 */
class GQLSimplifiedPathPatternNode;
using GQLSimplifiedPathPatternNodePtr = std::shared_ptr<GQLSimplifiedPathPatternNode>;

class GQLSimplifiedPathPatternNode final : public IQueryTreeNode {
 public:
  GQLSimplifiedPathPatternNode();

  /// Get expression (GQLSimplifiedPathExprNode)
  const QueryTreeNodePtr &getExpression() const { return children[expression_child_index]; }

  /// Get expression
  QueryTreeNodePtr &getExpression() { return children[expression_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_SIMPLIFIED_PATH_PATTERN; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t expression_child_index = 0;
  static constexpr size_t children_size = expression_child_index + 1;
};

/** GQL Simplified Path Expr Node represents simplified path expression.
 *
 * Example: -[r:KNOWS*1..5]->
 */
class GQLSimplifiedPathExprNode;
using GQLSimplifiedPathExprNodePtr = std::shared_ptr<GQLSimplifiedPathExprNode>;

class GQLSimplifiedPathExprNode final : public IQueryTreeNode {
 public:
  GQLSimplifiedPathExprNode();

  /// Get edge pattern (GQLEdgePatternNode)
  const QueryTreeNodePtr &getEdgePattern() const { return children[edge_pattern_child_index]; }

  /// Get edge pattern
  QueryTreeNodePtr &getEdgePattern() { return children[edge_pattern_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_SIMPLIFIED_PATH_EXPR; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t edge_pattern_child_index = 0;
  static constexpr size_t children_size = edge_pattern_child_index + 1;
};

/** GQL Optional Block Node represents block-level OPTIONAL { ... } semantics.
 *
 * Different from GQLMatchNode::optional which is clause-level OPTIONAL MATCH.
 *
 * Example: OPTIONAL { MATCH (n) WHERE n.age > 18 RETURN n }
 */
class GQLOptionalBlockNode;
using GQLOptionalBlockNodePtr = std::shared_ptr<GQLOptionalBlockNode>;

class GQLOptionalBlockNode final : public IQueryTreeNode {
 public:
  GQLOptionalBlockNode();

  /// Get inner query (GQLLinearQueryNode)
  const QueryTreeNodePtr &getInnerQuery() const { return children[inner_query_child_index]; }

  /// Get inner query
  QueryTreeNodePtr &getInnerQuery() { return children[inner_query_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_OPTIONAL_BLOCK; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t inner_query_child_index = 0;
  static constexpr size_t children_size = inner_query_child_index + 1;
};

/** GQL Order By Node represents ORDER BY clause.
 *
 * Example: ORDER BY n.age DESC, n.name ASC
 */
class GQLOrderByNode;
using GQLOrderByNodePtr = std::shared_ptr<GQLOrderByNode>;

class GQLOrderByNode final : public IQueryTreeNode {
 public:
  GQLOrderByNode();

  /// Get sort items (list of SortNode)
  const ListNode &getSortItems() const { return children[sort_items_child_index]->as<const ListNode &>(); }

  /// Get sort items
  ListNode &getSortItems() { return children[sort_items_child_index]->as<ListNode &>(); }

  /// Get sort items node
  const QueryTreeNodePtr &getSortItemsNode() const { return children[sort_items_child_index]; }

  /// Get sort items node
  QueryTreeNodePtr &getSortItemsNode() { return children[sort_items_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_ORDER_BY; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t sort_items_child_index = 0;
  static constexpr size_t children_size = sort_items_child_index + 1;
};

/** GQL Page Node represents OFFSET/LIMIT clause.
 *
 * Example: OFFSET 10 LIMIT 20
 */
class GQLPageNode;
using GQLPageNodePtr = std::shared_ptr<GQLPageNode>;

class GQLPageNode final : public IQueryTreeNode {
 public:
  GQLPageNode();

  /// Get offset expression (ConstantNode or nullptr)
  const QueryTreeNodePtr &getOffset() const { return children[offset_child_index]; }

  /// Get offset expression
  QueryTreeNodePtr &getOffset() { return children[offset_child_index]; }

  /// Get limit expression (ConstantNode or nullptr)
  const QueryTreeNodePtr &getLimit() const { return children[limit_child_index]; }

  /// Get limit expression
  QueryTreeNodePtr &getLimit() { return children[limit_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PAGE; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t offset_child_index = 0;
  static constexpr size_t limit_child_index = 1;
  static constexpr size_t children_size = limit_child_index + 1;
};

/** GQL Node Pattern Node represents a node pattern in a path.
 *
 * Example: (n:Person {age: 18})
 * Example: (n:Person|Company WHERE n.age > 18)
 */
class GQLNodePatternNode;
using GQLNodePatternNodePtr = std::shared_ptr<GQLNodePatternNode>;

class GQLNodePatternNode final : public IQueryTreeNode {
 public:
  GQLNodePatternNode();

  /// Get element variable name (e.g., 'n' in '(n:Person)')
  const String &getElementVariable() const { return element_variable; }

  /// Set element variable name
  void setElementVariable(String element_variable_value) { element_variable = std::move(element_variable_value); }

  /// Get label expression (GQLLabelExpressionNode or nullptr)
  const QueryTreeNodePtr &getLabelExpression() const { return children[label_expression_child_index]; }

  /// Get label expression
  QueryTreeNodePtr &getLabelExpression() { return children[label_expression_child_index]; }

  /// Get property map (GQLPropertyMapNode or nullptr)
  const QueryTreeNodePtr &getPropertyMap() const { return children[property_map_child_index]; }

  /// Get property map
  QueryTreeNodePtr &getPropertyMap() { return children[property_map_child_index]; }

  /// Get WHERE predicate (expression or nullptr)
  const QueryTreeNodePtr &getWhere() const { return children[where_child_index]; }

  /// Get WHERE predicate
  QueryTreeNodePtr &getWhere() { return children[where_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_NODE_PATTERN; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  String element_variable;

  static constexpr size_t label_expression_child_index = 0;
  static constexpr size_t property_map_child_index = 1;
  static constexpr size_t where_child_index = 2;
  static constexpr size_t children_size = where_child_index + 1;
};

/** GQL Edge Pattern Node represents an edge pattern in a path.
 *
 * Example: -[r:KNOWS]->
 * Example: -[r:KNOWS|FOLLOWS*1..5 {since: 2020}]->
 */
class GQLEdgePatternNode;
using GQLEdgePatternNodePtr = std::shared_ptr<GQLEdgePatternNode>;

class GQLEdgePatternNode final : public IQueryTreeNode {
 public:
  enum class Direction : uint8_t {
    Left,               // <-
    Right,              // ->
    Undirected,         // -
    LeftOrRight,        // <->
    LeftOrUndirected,   // <~
    UndirectedOrRight,  // ~>
    Any,                // ~
  };

  GQLEdgePatternNode();

  /// Get element variable name (e.g., 'r' in '-[r:KNOWS]->')
  const String &getElementVariable() const { return element_variable; }

  /// Set element variable name
  void setElementVariable(String element_variable_value) { element_variable = std::move(element_variable_value); }

  /// Get edge direction
  Direction getDirection() const { return direction; }

  /// Set edge direction
  void setDirection(Direction direction_value) { direction = direction_value; }

  /// Get label expression (GQLLabelExpressionNode or nullptr)
  const QueryTreeNodePtr &getLabelExpression() const { return children[label_expression_child_index]; }

  /// Get label expression
  QueryTreeNodePtr &getLabelExpression() { return children[label_expression_child_index]; }

  /// Get quantifier (GQLQuantifierNode or nullptr)
  const QueryTreeNodePtr &getQuantifier() const { return children[quantifier_child_index]; }

  /// Get quantifier
  QueryTreeNodePtr &getQuantifier() { return children[quantifier_child_index]; }

  /// Get property map (GQLPropertyMapNode or nullptr)
  const QueryTreeNodePtr &getPropertyMap() const { return children[property_map_child_index]; }

  /// Get property map
  QueryTreeNodePtr &getPropertyMap() { return children[property_map_child_index]; }

  /// Get WHERE predicate (expression or nullptr)
  const QueryTreeNodePtr &getWhere() const { return children[where_child_index]; }

  /// Get WHERE predicate
  QueryTreeNodePtr &getWhere() { return children[where_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_EDGE_PATTERN; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  String element_variable;
  Direction direction = Direction::Right;

  static constexpr size_t label_expression_child_index = 0;
  static constexpr size_t quantifier_child_index = 1;
  static constexpr size_t property_map_child_index = 2;
  static constexpr size_t where_child_index = 3;
  static constexpr size_t children_size = where_child_index + 1;
};

/** GQL Quantifier Node represents a quantifier for edge patterns.
 *
 * Example: *1..5
 * Example: *
 * Example: +
 */
class GQLQuantifierNode;
using GQLQuantifierNodePtr = std::shared_ptr<GQLQuantifierNode>;

class GQLQuantifierNode final : public IQueryTreeNode {
 public:
  GQLQuantifierNode();

  /// Get lower bound (ConstantNode or nullptr for unbounded)
  const QueryTreeNodePtr &getLowerBound() const { return children[lower_bound_child_index]; }

  /// Get lower bound
  QueryTreeNodePtr &getLowerBound() { return children[lower_bound_child_index]; }

  /// Get upper bound (ConstantNode or nullptr for unbounded)
  const QueryTreeNodePtr &getUpperBound() const { return children[upper_bound_child_index]; }

  /// Get upper bound
  QueryTreeNodePtr &getUpperBound() { return children[upper_bound_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_QUANTIFIER; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t lower_bound_child_index = 0;
  static constexpr size_t upper_bound_child_index = 1;
  static constexpr size_t children_size = upper_bound_child_index + 1;
};

/** GQL Label Expression Node represents a label expression.
 *
 * Example: Person
 * Example: Person|Company
 * Example: Person&Employee
 * Example: !Person
 */
class GQLLabelExpressionNode;
using GQLLabelExpressionNodePtr = std::shared_ptr<GQLLabelExpressionNode>;

class GQLLabelExpressionNode final : public IQueryTreeNode {
 public:
  enum class Operator : uint8_t {
    Label,  // Single label
    Or,     // |
    And,    // &
    Not,    // !
  };

  GQLLabelExpressionNode();

  /// Get operator
  Operator getOperator() const { return op; }

  /// Set operator
  void setOperator(Operator op_value) { op = op_value; }

  /// Get label name (for Label operator)
  const String &getLabelName() const { return label_name; }

  /// Set label name
  void setLabelName(String label_name_value) { label_name = std::move(label_name_value); }

  /// Get operands (for Or, And, Not operators)
  const ListNode &getOperands() const { return children[operands_child_index]->as<const ListNode &>(); }

  /// Get operands
  ListNode &getOperands() { return children[operands_child_index]->as<ListNode &>(); }

  /// Get operands node
  const QueryTreeNodePtr &getOperandsNode() const { return children[operands_child_index]; }

  /// Get operands node
  QueryTreeNodePtr &getOperandsNode() { return children[operands_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_LABEL_EXPRESSION; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  Operator op = Operator::Label;
  String label_name;

  static constexpr size_t operands_child_index = 0;
  static constexpr size_t children_size = operands_child_index + 1;
};

/** GQL Property Map Node represents a property map.
 *
 * Example: {age: 18, name: 'Alice'}
 */
class GQLPropertyMapNode;
using GQLPropertyMapNodePtr = std::shared_ptr<GQLPropertyMapNode>;

class GQLPropertyMapNode final : public IQueryTreeNode {
 public:
  GQLPropertyMapNode();

  /// Get property items (list of GQLPropertyItemNode)
  const ListNode &getItems() const { return children[items_child_index]->as<const ListNode &>(); }

  /// Get property items
  ListNode &getItems() { return children[items_child_index]->as<ListNode &>(); }

  /// Get items node
  const QueryTreeNodePtr &getItemsNode() const { return children[items_child_index]; }

  /// Get items node
  QueryTreeNodePtr &getItemsNode() { return children[items_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PROPERTY_MAP; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t items_child_index = 0;
  static constexpr size_t children_size = items_child_index + 1;
};

/** GQL Property Item Node represents a single property in a property map.
 *
 * Example: age: 18
 */
class GQLPropertyItemNode;
using GQLPropertyItemNodePtr = std::shared_ptr<GQLPropertyItemNode>;

class GQLPropertyItemNode final : public IQueryTreeNode {
 public:
  GQLPropertyItemNode();

  /// Get property name
  const String &getPropertyName() const { return property_name; }

  /// Set property name
  void setPropertyName(String property_name_value) { property_name = std::move(property_name_value); }

  /// Get property value expression
  const QueryTreeNodePtr &getValue() const { return children[value_child_index]; }

  /// Get property value expression
  QueryTreeNodePtr &getValue() { return children[value_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_PROPERTY_ITEM; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  String property_name;

  static constexpr size_t value_child_index = 0;
  static constexpr size_t children_size = value_child_index + 1;
};

/** GQL Keep Node represents a KEEP clause.
 *
 * Example: KEEP n, r
 */
class GQLKeepNode;
using GQLKeepNodePtr = std::shared_ptr<GQLKeepNode>;

class GQLKeepNode final : public IQueryTreeNode {
 public:
  GQLKeepNode();

  /// Get keep items (list of IdentifierNode or ColumnNode)
  const ListNode &getItems() const { return children[items_child_index]->as<const ListNode &>(); }

  /// Get keep items
  ListNode &getItems() { return children[items_child_index]->as<ListNode &>(); }

  /// Get items node
  const QueryTreeNodePtr &getItemsNode() const { return children[items_child_index]; }

  /// Get items node
  QueryTreeNodePtr &getItemsNode() { return children[items_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_KEEP; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t items_child_index = 0;
  static constexpr size_t children_size = items_child_index + 1;
};

/** GQL Yield Node represents a YIELD clause.
 *
 * Example: YIELD n.name AS name, r.since AS since
 */
class GQLYieldNode;
using GQLYieldNodePtr = std::shared_ptr<GQLYieldNode>;

class GQLYieldNode final : public IQueryTreeNode {
 public:
  GQLYieldNode();

  /// Get yield items (list of expressions with optional aliases)
  const ListNode &getItems() const { return children[items_child_index]->as<const ListNode &>(); }

  /// Get yield items
  ListNode &getItems() { return children[items_child_index]->as<ListNode &>(); }

  /// Get items node
  const QueryTreeNodePtr &getItemsNode() const { return children[items_child_index]; }

  /// Get items node
  QueryTreeNodePtr &getItemsNode() { return children[items_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_YIELD; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t items_child_index = 0;
  static constexpr size_t children_size = items_child_index + 1;
};

/** GQL Return Node represents a RETURN clause.
 *
 * Example: RETURN n.name, n.age
 * Example: RETURN DISTINCT n.name
 */
class GQLReturnNode;
using GQLReturnNodePtr = std::shared_ptr<GQLReturnNode>;

class GQLReturnNode final : public IQueryTreeNode {
 public:
  GQLReturnNode();

  /// Returns true if this is RETURN DISTINCT
  bool isDistinct() const { return distinct; }

  /// Set distinct flag
  void setDistinct(bool distinct_value) { distinct = distinct_value; }

  /// Get return items (list of expressions with optional aliases)
  const ListNode &getItems() const { return children[items_child_index]->as<const ListNode &>(); }

  /// Get return items
  ListNode &getItems() { return children[items_child_index]->as<ListNode &>(); }

  /// Get items node
  const QueryTreeNodePtr &getItemsNode() const { return children[items_child_index]; }

  /// Get items node
  QueryTreeNodePtr &getItemsNode() { return children[items_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_RETURN; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  bool distinct = false;

  static constexpr size_t items_child_index = 0;
  static constexpr size_t children_size = items_child_index + 1;
};

/** GQL Filter Node represents a FILTER clause.
 *
 * Example: FILTER n.age > 18
 */
class GQLFilterNode;
using GQLFilterNodePtr = std::shared_ptr<GQLFilterNode>;

class GQLFilterNode final : public IQueryTreeNode {
 public:
  GQLFilterNode();

  /// Get filter predicate expression
  const QueryTreeNodePtr &getPredicate() const { return children[predicate_child_index]; }

  /// Get filter predicate expression
  QueryTreeNodePtr &getPredicate() { return children[predicate_child_index]; }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_FILTER; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  static constexpr size_t predicate_child_index = 0;
  static constexpr size_t children_size = predicate_child_index + 1;
};

/** GQL Combined Query Node represents UNION/INTERSECT/EXCEPT of GQL queries.
 *
 * Corresponds to parser AST GQLCombinedQuery.
 *
 * Example: (MATCH (n) RETURN n) UNION ALL (MATCH (m) RETURN m)
 */
class GQLCombinedQueryNode;
using GQLCombinedQueryNodePtr = std::shared_ptr<GQLCombinedQueryNode>;

class GQLCombinedQueryNode final : public IQueryTreeNode {
 public:
  enum class CombinedOperator : uint8_t {
    UNION_ALL,
    UNION_DISTINCT,
    EXCEPT_ALL,
    EXCEPT_DISTINCT,
    INTERSECT_ALL,
    INTERSECT_DISTINCT,
  };

  GQLCombinedQueryNode();

  /// Get queries
  const ListNode &getQueries() const { return children[queries_child_index]->as<const ListNode &>(); }

  /// Get queries
  ListNode &getQueries() { return children[queries_child_index]->as<ListNode &>(); }

  /// Get queries node
  const QueryTreeNodePtr &getQueriesNode() const { return children[queries_child_index]; }

  /// Get queries node
  QueryTreeNodePtr &getQueriesNode() { return children[queries_child_index]; }

  /// Get operators
  const std::vector<CombinedOperator> &getOperators() const { return operators; }

  /// Set operators
  void setOperators(std::vector<CombinedOperator> operators_value) { operators = std::move(operators_value); }

  QueryTreeNodeType getNodeType() const override { return QueryTreeNodeType::GQL_COMBINED_QUERY; }

  void dumpTreeImpl(WriteBuffer &buffer, FormatState &format_state, size_t indent) const override;

 protected:
  bool isEqualImpl(const IQueryTreeNode &rhs, CompareOptions) const override;

  void updateTreeHashImpl(HashState &hash_state, CompareOptions) const override;

  QueryTreeNodePtr cloneImpl() const override;

  ASTPtr toASTImpl(const ConvertToASTOptions &options) const override;

 private:
  std::vector<CombinedOperator> operators;

  static constexpr size_t queries_child_index = 0;
  static constexpr size_t children_size = queries_child_index + 1;
};

}  // namespace DB
