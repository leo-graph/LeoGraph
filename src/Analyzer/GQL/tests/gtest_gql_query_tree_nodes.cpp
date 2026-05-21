#include <gtest/gtest.h>

#include <Analyzer/GQL/GQLQueryTreeNodes.h>
#include <IO/WriteBufferFromString.h>

using namespace DB;

TEST(GQLQueryTreeNodes, GQLLinearQueryNodeBasic)
{
    auto node = std::make_shared<GQLLinearQueryNode>();

    EXPECT_EQ(node->getNodeType(), QueryTreeNodeType::GQL_LINEAR_QUERY);
    EXPECT_EQ(node->getSteps().getNodes().size(), 0);

    // Test clone
    auto cloned = node->clone();
    EXPECT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->getNodeType(), QueryTreeNodeType::GQL_LINEAR_QUERY);

    // Test equality
    EXPECT_TRUE(node->isEqual(*cloned));

    // Test dump
    WriteBufferFromOwnString buffer;
    IQueryTreeNode::FormatState format_state;
    node->dumpTreeImpl(buffer, format_state, 0);
    String dump_result = buffer.str();
    EXPECT_TRUE(dump_result.find("GQL_LINEAR_QUERY") != String::npos);
}

TEST(GQLQueryTreeNodes, GQLMatchNodeBasic)
{
    auto node = std::make_shared<GQLMatchNode>();

    EXPECT_EQ(node->getNodeType(), QueryTreeNodeType::GQL_MATCH);
    EXPECT_FALSE(node->isOptional());
    EXPECT_EQ(node->getMatchMode(), OPENGQL::AST::GraphMatchMode::None);

    // Test setters
    node->setOptional(true);
    EXPECT_TRUE(node->isOptional());

    node->setMatchMode(OPENGQL::AST::GraphMatchMode::DifferentEdges);
    EXPECT_EQ(node->getMatchMode(), OPENGQL::AST::GraphMatchMode::DifferentEdges);

    // Test clone
    auto cloned = std::dynamic_pointer_cast<GQLMatchNode>(node->clone());
    EXPECT_NE(cloned, nullptr);
    EXPECT_TRUE(cloned->isOptional());
    EXPECT_EQ(cloned->getMatchMode(), OPENGQL::AST::GraphMatchMode::DifferentEdges);

    // Test equality
    EXPECT_TRUE(node->isEqual(*cloned));

    // Test dump
    WriteBufferFromOwnString buffer;
    IQueryTreeNode::FormatState format_state;
    node->dumpTreeImpl(buffer, format_state, 0);
    String dump_result = buffer.str();
    EXPECT_TRUE(dump_result.find("GQL_MATCH") != String::npos);
    EXPECT_TRUE(dump_result.find("optional: true") != String::npos);
    EXPECT_TRUE(dump_result.find("DIFFERENT_EDGES") != String::npos);
}

TEST(GQLQueryTreeNodes, GQLPathPatternNodeBasic)
{
    auto node = std::make_shared<GQLPathPatternNode>();

    EXPECT_EQ(node->getNodeType(), QueryTreeNodeType::GQL_PATH_PATTERN);
    EXPECT_TRUE(node->getPathVariable().empty());
    EXPECT_TRUE(node->getPrefix().empty());

    // Test setters
    node->setPathVariable("p");
    node->setPrefix("ANY SHORTEST");

    EXPECT_EQ(node->getPathVariable(), "p");
    EXPECT_EQ(node->getPrefix(), "ANY SHORTEST");

    // Test clone
    auto cloned = std::dynamic_pointer_cast<GQLPathPatternNode>(node->clone());
    EXPECT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->getPathVariable(), "p");
    EXPECT_EQ(cloned->getPrefix(), "ANY SHORTEST");

    // Test equality
    EXPECT_TRUE(node->isEqual(*cloned));

    // Test dump
    WriteBufferFromOwnString buffer;
    IQueryTreeNode::FormatState format_state;
    node->dumpTreeImpl(buffer, format_state, 0);
    String dump_result = buffer.str();
    EXPECT_TRUE(dump_result.find("GQL_PATH_PATTERN") != String::npos);
    EXPECT_TRUE(dump_result.find("path_variable: p") != String::npos);
    EXPECT_TRUE(dump_result.find("prefix: ANY SHORTEST") != String::npos);
}

TEST(GQLQueryTreeNodes, GQLCombinedQueryNodeBasic)
{
    auto node = std::make_shared<GQLCombinedQueryNode>();

    EXPECT_EQ(node->getNodeType(), QueryTreeNodeType::GQL_COMBINED_QUERY);
    EXPECT_EQ(node->getQueries().getNodes().size(), 0);
    EXPECT_EQ(node->getOperators().size(), 0);

    // Test setters
    std::vector<GQLCombinedQueryNode::CombinedOperator> ops = {
        GQLCombinedQueryNode::CombinedOperator::UNION_ALL,
        GQLCombinedQueryNode::CombinedOperator::INTERSECT_DISTINCT
    };
    node->setOperators(ops);

    EXPECT_EQ(node->getOperators().size(), 2);
    EXPECT_EQ(node->getOperators()[0], GQLCombinedQueryNode::CombinedOperator::UNION_ALL);
    EXPECT_EQ(node->getOperators()[1], GQLCombinedQueryNode::CombinedOperator::INTERSECT_DISTINCT);

    // Test clone
    auto cloned = std::dynamic_pointer_cast<GQLCombinedQueryNode>(node->clone());
    EXPECT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->getOperators().size(), 2);

    // Test equality
    EXPECT_TRUE(node->isEqual(*cloned));

    // Test dump
    WriteBufferFromOwnString buffer;
    IQueryTreeNode::FormatState format_state;
    node->dumpTreeImpl(buffer, format_state, 0);
    String dump_result = buffer.str();
    EXPECT_TRUE(dump_result.find("GQL_COMBINED_QUERY") != String::npos);
    EXPECT_TRUE(dump_result.find("UNION_ALL") != String::npos);
    EXPECT_TRUE(dump_result.find("INTERSECT_DISTINCT") != String::npos);
}

TEST(GQLQueryTreeNodes, GQLLinearQueryWithSteps)
{
    auto linear_query = std::make_shared<GQLLinearQueryNode>();
    auto match_node = std::make_shared<GQLMatchNode>();

    // Add match node as a step
    linear_query->getSteps().getNodes().push_back(match_node);

    EXPECT_EQ(linear_query->getSteps().getNodes().size(), 1);
    EXPECT_EQ(linear_query->getSteps().getNodes()[0]->getNodeType(), QueryTreeNodeType::GQL_MATCH);

    // Test dump with children
    WriteBufferFromOwnString buffer;
    IQueryTreeNode::FormatState format_state;
    linear_query->dumpTreeImpl(buffer, format_state, 0);
    String dump_result = buffer.str();
    EXPECT_TRUE(dump_result.find("GQL_LINEAR_QUERY") != String::npos);
    EXPECT_TRUE(dump_result.find("STEPS") != String::npos);
}
