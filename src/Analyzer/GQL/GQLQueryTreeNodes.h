#pragma once

#include <Analyzer/IQueryTreeNode.h>
#include <Analyzer/ListNode.h>
#include <Parsers/graph/AST/Utils.h>

namespace DB
{

namespace OPENGQL::AST
{
class GQLSingleQuery;
class GQLCombinedQuery;
class GQLMatchClause;
class GQLPathPattern;
}

/** GQL Linear Query Node represents a single GQL query with ordered clause pipeline.
 *
 * Corresponds to parser AST GQLSingleQuery but stores semantic steps rather than raw grammar nodes.
 *
 * Example: MATCH (n) WHERE n.age > 18 RETURN n ORDER BY n.age LIMIT 10
 */
class GQLLinearQueryNode;
using GQLLinearQueryNodePtr = std::shared_ptr<GQLLinearQueryNode>;

class GQLLinearQueryNode final : public IQueryTreeNode
{
public:
    GQLLinearQueryNode();

    /// Get ordered clause steps
    const ListNode & getSteps() const
    {
        return children[steps_child_index]->as<const ListNode &>();
    }

    /// Get ordered clause steps
    ListNode & getSteps()
    {
        return children[steps_child_index]->as<ListNode &>();
    }

    /// Get steps node
    const QueryTreeNodePtr & getStepsNode() const
    {
        return children[steps_child_index];
    }

    /// Get steps node
    QueryTreeNodePtr & getStepsNode()
    {
        return children[steps_child_index];
    }

    QueryTreeNodeType getNodeType() const override
    {
        return QueryTreeNodeType::GQL_LINEAR_QUERY;
    }

    void dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const override;

protected:
    bool isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const override;

    void updateTreeHashImpl(HashState & hash_state, CompareOptions) const override;

    QueryTreeNodePtr cloneImpl() const override;

    ASTPtr toASTImpl(const ConvertToASTOptions & options) const override;

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

class GQLMatchNode final : public IQueryTreeNode
{
public:
    GQLMatchNode();

    /// Returns true if this is OPTIONAL MATCH
    bool isOptional() const
    {
        return optional;
    }

    /// Set optional flag
    void setOptional(bool optional_value)
    {
        optional = optional_value;
    }

    /// Get match mode
    OPENGQL::AST::GraphMatchMode getMatchMode() const
    {
        return match_mode;
    }

    /// Set match mode
    void setMatchMode(OPENGQL::AST::GraphMatchMode match_mode_value)
    {
        match_mode = match_mode_value;
    }

    /// Get path patterns
    const ListNode & getPathPatterns() const
    {
        return children[path_patterns_child_index]->as<const ListNode &>();
    }

    /// Get path patterns
    ListNode & getPathPatterns()
    {
        return children[path_patterns_child_index]->as<ListNode &>();
    }

    /// Get path patterns node
    const QueryTreeNodePtr & getPathPatternsNode() const
    {
        return children[path_patterns_child_index];
    }

    /// Get path patterns node
    QueryTreeNodePtr & getPathPatternsNode()
    {
        return children[path_patterns_child_index];
    }

    /// Get WHERE predicate
    const QueryTreeNodePtr & getWhere() const
    {
        return children[where_child_index];
    }

    /// Get WHERE predicate
    QueryTreeNodePtr & getWhere()
    {
        return children[where_child_index];
    }

    /// Get KEEP clause
    const QueryTreeNodePtr & getKeep() const
    {
        return children[keep_child_index];
    }

    /// Get KEEP clause
    QueryTreeNodePtr & getKeep()
    {
        return children[keep_child_index];
    }

    /// Get YIELD items
    const QueryTreeNodePtr & getYield() const
    {
        return children[yield_child_index];
    }

    /// Get YIELD items
    QueryTreeNodePtr & getYield()
    {
        return children[yield_child_index];
    }

    QueryTreeNodeType getNodeType() const override
    {
        return QueryTreeNodeType::GQL_MATCH;
    }

    void dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const override;

protected:
    bool isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const override;

    void updateTreeHashImpl(HashState & hash_state, CompareOptions) const override;

    QueryTreeNodePtr cloneImpl() const override;

    ASTPtr toASTImpl(const ConvertToASTOptions & options) const override;

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

class GQLPathPatternNode final : public IQueryTreeNode
{
public:
    GQLPathPatternNode();

    /// Get path variable name (e.g., 'p' in 'p = (a)-[r]->(b)')
    const String & getPathVariable() const
    {
        return path_variable;
    }

    /// Set path variable name
    void setPathVariable(String path_variable_value)
    {
        path_variable = std::move(path_variable_value);
    }

    /// Get path prefix (e.g., 'ANY SHORTEST')
    const String & getPrefix() const
    {
        return prefix;
    }

    /// Set path prefix
    void setPrefix(String prefix_value)
    {
        prefix = std::move(prefix_value);
    }

    /// Get path expression (GQLPathTermNode | GQLPathAlternationNode | GQLSimplifiedPathPatternNode)
    const QueryTreeNodePtr & getExpression() const
    {
        return children[expression_child_index];
    }

    /// Get path expression
    QueryTreeNodePtr & getExpression()
    {
        return children[expression_child_index];
    }

    QueryTreeNodeType getNodeType() const override
    {
        return QueryTreeNodeType::GQL_PATH_PATTERN;
    }

    void dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const override;

protected:
    bool isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const override;

    void updateTreeHashImpl(HashState & hash_state, CompareOptions) const override;

    QueryTreeNodePtr cloneImpl() const override;

    ASTPtr toASTImpl(const ConvertToASTOptions & options) const override;

private:
    String path_variable;
    String prefix;

    static constexpr size_t expression_child_index = 0;
    static constexpr size_t children_size = expression_child_index + 1;
};

/** GQL Combined Query Node represents UNION/INTERSECT/EXCEPT of GQL queries.
 *
 * Corresponds to parser AST GQLCombinedQuery.
 *
 * Example: (MATCH (n) RETURN n) UNION ALL (MATCH (m) RETURN m)
 */
class GQLCombinedQueryNode;
using GQLCombinedQueryNodePtr = std::shared_ptr<GQLCombinedQueryNode>;

class GQLCombinedQueryNode final : public IQueryTreeNode
{
public:
    enum class CombinedOperator : uint8_t
    {
        UNION_ALL,
        UNION_DISTINCT,
        EXCEPT_ALL,
        EXCEPT_DISTINCT,
        INTERSECT_ALL,
        INTERSECT_DISTINCT,
    };

    GQLCombinedQueryNode();

    /// Get queries
    const ListNode & getQueries() const
    {
        return children[queries_child_index]->as<const ListNode &>();
    }

    /// Get queries
    ListNode & getQueries()
    {
        return children[queries_child_index]->as<ListNode &>();
    }

    /// Get queries node
    const QueryTreeNodePtr & getQueriesNode() const
    {
        return children[queries_child_index];
    }

    /// Get queries node
    QueryTreeNodePtr & getQueriesNode()
    {
        return children[queries_child_index];
    }

    /// Get operators
    const std::vector<CombinedOperator> & getOperators() const
    {
        return operators;
    }

    /// Set operators
    void setOperators(std::vector<CombinedOperator> operators_value)
    {
        operators = std::move(operators_value);
    }

    QueryTreeNodeType getNodeType() const override
    {
        return QueryTreeNodeType::GQL_COMBINED_QUERY;
    }

    void dumpTreeImpl(WriteBuffer & buffer, FormatState & format_state, size_t indent) const override;

protected:
    bool isEqualImpl(const IQueryTreeNode & rhs, CompareOptions) const override;

    void updateTreeHashImpl(HashState & hash_state, CompareOptions) const override;

    QueryTreeNodePtr cloneImpl() const override;

    ASTPtr toASTImpl(const ConvertToASTOptions & options) const override;

private:
    std::vector<CombinedOperator> operators;

    static constexpr size_t queries_child_index = 0;
    static constexpr size_t children_size = queries_child_index + 1;
};

}
