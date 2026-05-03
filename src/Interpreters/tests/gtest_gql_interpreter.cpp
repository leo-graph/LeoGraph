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

TEST(GQLInterpreter, EdgePatternIsRejectedUntilStorageLands)
{
    try
    {
        (void)buildPlan("MATCH (a)-[r]->(b) RETURN a");
        FAIL() << "Expected edge MATCH pattern to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
    }
}

#endif
