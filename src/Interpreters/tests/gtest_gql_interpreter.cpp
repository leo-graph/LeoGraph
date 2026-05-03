#include "config.h"

#if USE_GQL_GRAMMAR

#  include <gtest/gtest.h>

#  include <Common/Exception.h>
#  include <Common/tests/gtest_global_context.h>
#  include <Common/tests/gtest_global_register.h>
#  include <Interpreters/InterpreterGQLQuery.h>
#  include <Parsers/graph/ParserGQLQuery.h>
#  include <Processors/QueryPlan/ExpressionStep.h>
#  include <Processors/QueryPlan/FilterStep.h>
#  include <Processors/QueryPlan/Graph/MatchStep.h>
#  include <Processors/QueryPlan/LimitStep.h>
#  include <Processors/QueryPlan/QueryPlan.h>

#  include <vector>

using namespace DB;

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace
{

ContextPtr getInterpreterContext()
{
    tryRegisterFunctions();
    return getContext().context;
}

ASTPtr parseGQL(const std::string & query)
{
    ParserGQLQuery parser;
    return parseGQLQuery(parser, query, 0, 0, 0);
}

QueryPlan buildPlan(const std::string & query)
{
    InterpreterGQLQuery interpreter(parseGQL(query), getInterpreterContext());
    QueryPlan plan;
    interpreter.buildQueryPlan(plan);
    return plan;
}

std::vector<String> linearStepNames(const QueryPlan & plan)
{
    std::vector<String> names;
    const auto * node = plan.getRootNode();
    while (node)
    {
        names.push_back(node->step->getName());
        if (node->children.size() != 1)
            break;
        node = node->children.front();
    }
    return names;
}

const Graph::MatchStep * leafMatchStep(const QueryPlan & plan)
{
    const auto * node = plan.getRootNode();
    while (node && node->children.size() == 1)
        node = node->children.front();

    if (!node)
        return nullptr;

    return dynamic_cast<const Graph::MatchStep *>(node->step.get());
}

}

TEST(GQLInterpreter, BareMatchReturnLowersToScanThenProjection)
{
    const auto plan = buildPlan("MATCH (n) RETURN n");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);
    ASSERT_EQ(match.paths.front().nodes.size(), 1u);
    EXPECT_EQ(match.paths.front().nodes.front().variable, "n");
    EXPECT_TRUE(match.paths.front().edges.empty());
}

TEST(GQLInterpreter, MatchWhereReturnLimitChainsAllSteps)
{
    const auto plan = buildPlan("MATCH (n) WHERE n = 1 RETURN n LIMIT 5");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Limit", "Expression", "Filter", "GraphMatch"}));
}

TEST(GQLInterpreter, MatchFilterClauseLowersToStandaloneFilter)
{
    const auto plan = buildPlan("MATCH (n) FILTER n = 1 RETURN n");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Filter", "GraphMatch"}));
}

TEST(GQLInterpreter, ParsedSpecialValuePredicateLowersToFilter)
{
    const auto plan = buildPlan("MATCH (n) WHERE TRUE RETURN n");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Filter", "GraphMatch"}));
}

TEST(GQLInterpreter, IsNullPredicateLowersToFilter)
{
    const auto plan = buildPlan("MATCH (n) WHERE n IS NULL RETURN n");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Filter", "GraphMatch"}));
}

TEST(GQLInterpreter, ReturnAliasIsPreservedInProjection)
{
    const auto plan = buildPlan("MATCH (n) RETURN n AS node_id");

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "node_id");
}

TEST(GQLInterpreter, UnsupportedClauseThrowsNotImplemented)
{
    try
    {
        (void)buildPlan("MATCH (n) RETURN DISTINCT n");
        FAIL() << "Expected RETURN DISTINCT to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find("DISTINCT"), String::npos);
    }
}

TEST(GQLInterpreter, EdgePatternLowersToGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)-[r]->(b) RETURN a, r, b");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);

    const auto & path = match.paths.front();
    ASSERT_EQ(path.nodes.size(), 2u);
    ASSERT_EQ(path.edges.size(), 1u);
    EXPECT_EQ(path.nodes[0].variable, "a");
    EXPECT_EQ(path.edges[0].variable, "r");
    EXPECT_EQ(path.edges[0].direction, Graph::MatchEdgeDirection::Outgoing);
    EXPECT_EQ(path.nodes[1].variable, "b");
}

TEST(GQLInterpreter, PathVariableStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH p = (a)-[r]->(b) RETURN a, r, b");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);
    EXPECT_EQ(match.paths.front().variable, "p");
}

TEST(GQLInterpreter, CompoundEdgeDirectionLowersToGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)<-[r]->(b) RETURN a, r, b");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);
    ASSERT_EQ(match.paths.front().edges.size(), 1u);
    EXPECT_EQ(match.paths.front().edges.front().direction, Graph::MatchEdgeDirection::IncomingOrOutgoing);
}

TEST(GQLInterpreter, MatchPatternConstraintsStayInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (n:Person {name: 'x'}) RETURN n");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);
    ASSERT_EQ(match.paths.front().nodes.size(), 1u);

    const auto & node = match.paths.front().nodes.front();
    EXPECT_EQ(node.variable, "n");
    EXPECT_NE(node.label_expression, nullptr);
    EXPECT_NE(node.properties, nullptr);

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    const auto & cloned_node = cloned_match_step->getMatchSpec().paths.front().nodes.front();
    EXPECT_NE(cloned_node.label_expression.get(), node.label_expression.get());
    EXPECT_NE(cloned_node.properties.get(), node.properties.get());
}

#endif
