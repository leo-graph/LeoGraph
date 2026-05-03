#include "config.h"

#if USE_GQL_GRAMMAR

#  include <gtest/gtest.h>

#  include <Common/Exception.h>
#  include <Common/tests/gtest_global_context.h>
#  include <Common/tests/gtest_global_register.h>
#  include <Interpreters/GQL/PlanBuilder.h>
#  include <Interpreters/InterpreterGQLQuery.h>
#  include <Parsers/graph/GraphAST.h>
#  include <Parsers/graph/ParserGQLQuery.h>
#  include <Processors/QueryPlan/ExpressionStep.h>
#  include <Processors/QueryPlan/FilterStep.h>
#  include <Processors/QueryPlan/Graph/MatchStep.h>
#  include <Processors/QueryPlan/LimitStep.h>
#  include <Processors/QueryPlan/QueryPlan.h>

#  include <vector>

using namespace DB;

namespace GAST = DB::OPENGQL::AST;

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

QueryPlan buildPlanWithPlanBuilder(const std::string & query)
{
    const auto ast = parseGQL(query);
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "test query must parse to GQLSingleQuery");

    QueryPlan plan;
    GQL::PlanBuilder(getInterpreterContext()).buildSingleQuery(plan, *single_query);
    return plan;
}

std::vector<GQL::PlanBinding> buildScopeBindingsWithPlanBuilder(const std::string & query)
{
    const auto ast = parseGQL(query);
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "test query must parse to GQLSingleQuery");

    QueryPlan plan;
    GQL::PlanBuilder builder(getInterpreterContext());
    builder.buildSingleQuery(plan, *single_query);
    return builder.getScope().getBindings();
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

TEST(GQLInterpreter, PlanBuilderLowersSingleQueryWithoutInterpreter)
{
    const auto plan = buildPlanWithPlanBuilder("MATCH (n) WHERE TRUE RETURN n LIMIT 1");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Limit", "Expression", "Filter", "GraphMatch"}));
}

TEST(GQLInterpreter, PlanBuilderScopeTracksMatchSourceBindings)
{
    const auto bindings = buildScopeBindingsWithPlanBuilder("MATCH (a)-[r]->(b) RETURN *");

    ASSERT_EQ(bindings.size(), 3u);
    EXPECT_EQ(bindings[0].name, "a");
    EXPECT_EQ(bindings[1].name, "r");
    EXPECT_EQ(bindings[2].name, "b");
    EXPECT_EQ(bindings[0].kind, GQL::BindingKind::Source);
    EXPECT_EQ(bindings[1].kind, GQL::BindingKind::Source);
    EXPECT_EQ(bindings[2].kind, GQL::BindingKind::Source);
}

TEST(GQLInterpreter, PlanBuilderScopeTracksProjectionBindings)
{
    const auto bindings = buildScopeBindingsWithPlanBuilder("MATCH (n) RETURN n AS node_id");

    ASSERT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings[0].name, "node_id");
    EXPECT_EQ(bindings[0].kind, GQL::BindingKind::Projection);
    ASSERT_NE(bindings[0].type, nullptr);
}

TEST(GQLInterpreter, ReturnWithoutMatchUsesReusableProjectionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN 1 AS one");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "one");
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

TEST(GQLInterpreter, UnknownProjectionIdentifierUsesPlanScope)
{
    try
    {
        (void)buildPlan("MATCH (n) RETURN missing");
        FAIL() << "Expected unknown projection identifier to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find("current plan scope"), String::npos);
    }
}

TEST(GQLInterpreter, PipelineClauseBeforeSourceThrowsNotImplemented)
{
    try
    {
        (void)buildPlan("FILTER TRUE RETURN 1");
        FAIL() << "Expected FILTER before a source clause to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find("source clause"), String::npos);
    }
}

TEST(GQLInterpreter, OptionalMatchOperandBlockStillRequiresExecutionSemantics)
{
    try
    {
        (void)buildPlan("OPTIONAL { MATCH (a) } RETURN *");
        FAIL() << "Expected OPTIONAL MATCH operand block to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find("OPTIONAL MATCH"), String::npos);
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

TEST(GQLInterpreter, PathAlternationLowersToGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)-[r]->(b) | (c)-[s]->(d) RETURN a, r, b, c, s, d");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);

    const auto & path = match.paths.front();
    EXPECT_EQ(path.alternation_kind, Graph::MatchPathAlternationKind::Union);
    ASSERT_EQ(path.alternatives.size(), 2u);
    ASSERT_EQ(path.alternatives[0].nodes.size(), 2u);
    ASSERT_EQ(path.alternatives[0].edges.size(), 1u);
    ASSERT_EQ(path.alternatives[1].nodes.size(), 2u);
    ASSERT_EQ(path.alternatives[1].edges.size(), 1u);

    const auto & header = *match_step->getOutputHeader();
    ASSERT_EQ(header.columns(), 6u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
    EXPECT_EQ(header.getByPosition(1).name, "r");
    EXPECT_EQ(header.getByPosition(2).name, "b");
    EXPECT_EQ(header.getByPosition(3).name, "c");
    EXPECT_EQ(header.getByPosition(4).name, "s");
    EXPECT_EQ(header.getByPosition(5).name, "d");

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    const auto & cloned_path = cloned_match_step->getMatchSpec().paths.front();
    ASSERT_EQ(cloned_path.alternatives.size(), 2u);
    EXPECT_EQ(cloned_path.alternatives[0].edges.front().variable, "r");
    EXPECT_EQ(cloned_path.alternatives[1].edges.front().variable, "s");
}

TEST(GQLInterpreter, EdgeQuantifierStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)-[r]->{2,5}(b) RETURN a, r, b");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);
    ASSERT_EQ(match.paths.front().edges.size(), 1u);

    const auto & edge = match.paths.front().edges.front();
    ASSERT_NE(edge.quantifier, nullptr);
    const auto * quantifier = edge.quantifier->as<GAST::GQLQuantifier>();
    ASSERT_NE(quantifier, nullptr);
    EXPECT_EQ(quantifier->kind, GAST::GQLQuantifier::Kind::Range);
    EXPECT_EQ(quantifier->lower, "2");
    EXPECT_EQ(quantifier->upper, "5");

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    const auto & cloned_edge = cloned_match_step->getMatchSpec().paths.front().edges.front();
    EXPECT_NE(cloned_edge.quantifier.get(), edge.quantifier.get());
}

TEST(GQLInterpreter, PathPrefixStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH ANY SHORTEST (a)-[r]->(b) RETURN a, b");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.paths.size(), 1u);

    const auto & path = match.paths.front();
    ASSERT_NE(path.prefix, nullptr);
    const auto * prefix = path.prefix->as<GAST::GQLPathSearchPrefix>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->search_kind, GAST::PathSearchKind::AnyShortest);

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    const auto & cloned_path = cloned_match_step->getMatchSpec().paths.front();
    EXPECT_NE(cloned_path.prefix.get(), path.prefix.get());
}

TEST(GQLInterpreter, KeepClauseStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)-[r]->(b) KEEP ANY 2 PATHS RETURN a");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_TRUE(match.has_keep_clause);
    ASSERT_NE(match.keep_clause, nullptr);

    const auto * keep = match.keep_clause->as<GAST::GQLKeepClause>();
    ASSERT_NE(keep, nullptr);
    ASSERT_NE(keep->path_prefix, nullptr);
    const auto * prefix = keep->path_prefix->as<GAST::GQLPathSearchPrefix>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->search_kind, GAST::PathSearchKind::Any);
    EXPECT_EQ(prefix->count_kind, GAST::CountKind::Paths);

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    EXPECT_NE(cloned_match_step->getMatchSpec().keep_clause.get(), match.keep_clause.get());
}

TEST(GQLInterpreter, MatchWhereClauseStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH (a)-[r]->(b) WHERE a IS NOT NULL RETURN a");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Filter", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_NE(match.where_clause, nullptr);

    const auto * where = match.where_clause->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);
    EXPECT_EQ(where->type, GAST::GQLWhereClause::Type::Where);
    ASSERT_NE(where->expression, nullptr);

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    EXPECT_NE(cloned_match_step->getMatchSpec().where_clause.get(), match.where_clause.get());
}

TEST(GQLInterpreter, MatchModeStaysInGraphMatchSpec)
{
    const auto plan = buildPlan("MATCH DIFFERENT EDGES (a)-[r]->(b) RETURN a");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    EXPECT_TRUE(match.has_match_mode);
    EXPECT_EQ(match.match_mode, Graph::MatchMode::DifferentEdges);

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    EXPECT_EQ(cloned_match_step->getMatchSpec().match_mode, Graph::MatchMode::DifferentEdges);
}

TEST(GQLInterpreter, MatchYieldRestrictsGraphMatchHeader)
{
    const auto plan = buildPlan("MATCH (a)-[r]->(b) YIELD r RETURN r");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_TRUE(match.has_yield_items);
    ASSERT_EQ(match.yield_variables.size(), 1u);
    EXPECT_EQ(match.yield_variables.front(), "r");

    const auto & header = *match_step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "r");

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    ASSERT_EQ(cloned_match_step->getMatchSpec().yield_items.size(), 1u);
    EXPECT_NE(cloned_match_step->getMatchSpec().yield_items.front().get(), match.yield_items.front().get());
}

TEST(GQLInterpreter, ConsecutiveMatchClausesShareGraphMatchStep)
{
    const auto plan = buildPlan("MATCH (a) MATCH (b) RETURN a, b");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    const auto & match = match_step->getMatchSpec();
    ASSERT_EQ(match.clauses.size(), 2u);
    ASSERT_EQ(match.paths.size(), 2u);

    const auto & header = *match_step->getOutputHeader();
    ASSERT_EQ(header.columns(), 2u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
    EXPECT_EQ(header.getByPosition(1).name, "b");
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
