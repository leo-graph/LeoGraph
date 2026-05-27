#include <gtest/gtest.h>

#include <Analyzer/GQL/GQLQueryTreeBuilder.h>
#include <Analyzer/GQL/GQLLinearQueryNode.h>
#include <Analyzer/GQL/GQLCombinedQueryNode.h>
#include <Analyzer/GQL/GQLMatchNode.h>
#include <Interpreters/Context.h>
#include <Parsers/graph/ParserGQLQuery.h>
#include <Parsers/graph/GraphAST.h>

using namespace DB;
using namespace DB::GQL;

namespace
{

ContextMutablePtr getContext()
{
    static ContextMutablePtr context = Context::createGlobal(nullptr);
    return context;
}

ASTPtr parseGQL(const std::string & query)
{
    ParserGQLQuery parser;
    return parser.parse(query);
}

}

TEST(GQLQueryTreeBuilder, SimpleMatch)
{
    // Parse: MATCH (n) RETURN n
    auto ast = parseGQL("MATCH (n) RETURN n");
    ASSERT_NE(ast, nullptr);

    // Build QueryTree
    auto query_tree = buildGQLQueryTree(*ast, getContext());
    ASSERT_NE(query_tree, nullptr);

    // Verify it's a GQLLinearQueryNode
    auto * linear_node = query_tree->as<GQLLinearQueryNode>();
    ASSERT_NE(linear_node, nullptr);

    // Verify it has steps
    const auto & steps = linear_node->getSteps().getNodes();
    EXPECT_GE(steps.size(), 1u);

    // First step should be MATCH
    if (!steps.empty())
    {
        auto * match_node = steps[0]->as<GQLMatchNode>();
        EXPECT_NE(match_node, nullptr);
    }
}

TEST(GQLQueryTreeBuilder, CombinedQuery)
{
    // Parse: MATCH (n) RETURN n UNION ALL MATCH (m) RETURN m
    auto ast = parseGQL("MATCH (n) RETURN n UNION ALL MATCH (m) RETURN m");
    ASSERT_NE(ast, nullptr);

    // Build QueryTree
    auto query_tree = buildGQLQueryTree(*ast, getContext());
    ASSERT_NE(query_tree, nullptr);

    // Verify it's a GQLCombinedQueryNode
    auto * combined_node = query_tree->as<GQLCombinedQueryNode>();
    ASSERT_NE(combined_node, nullptr);

    // Verify it has 2 subqueries
    const auto & queries = combined_node->getQueries().getNodes();
    EXPECT_EQ(queries.size(), 2u);

    // Verify it has 1 operator (UNION ALL)
    const auto & operators = combined_node->getOperators();
    EXPECT_EQ(operators.size(), 1u);
    if (!operators.empty())
    {
        EXPECT_EQ(operators[0], GQLCombinedQueryNode::CombinedOperator::UNION_ALL);
    }
}

TEST(GQLQueryTreeBuilder, RoundTrip)
{
    // Parse -> QueryTree -> AST -> QueryTree should be idempotent
    auto original_ast = parseGQL("MATCH (n) RETURN n");
    ASSERT_NE(original_ast, nullptr);

    // Build QueryTree
    auto query_tree = buildGQLQueryTree(*original_ast, getContext());
    ASSERT_NE(query_tree, nullptr);

    // Convert back to AST
    auto reconstructed_ast = query_tree->toAST();
    ASSERT_NE(reconstructed_ast, nullptr);

    // Build QueryTree again
    auto query_tree2 = buildGQLQueryTree(*reconstructed_ast, getContext());
    ASSERT_NE(query_tree2, nullptr);

    // Both QueryTrees should have the same structure
    EXPECT_EQ(query_tree->getNodeType(), query_tree2->getNodeType());
}
