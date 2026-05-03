#include "config.h"

#if USE_GQL_GRAMMAR

#  include <gtest/gtest.h>

#  include <Common/Exception.h>
#  include <Common/tests/gtest_global_context.h>
#  include <Common/tests/gtest_global_register.h>
#  include <DataTypes/DataTypesNumber.h>
#  include <Interpreters/GQL/AggregationLowering.h>
#  include <Interpreters/GQL/ClauseLowering.h>
#  include <Interpreters/GQL/ExpressionLowering.h>
#  include <Interpreters/GQL/PlanBuilder.h>
#  include <Interpreters/InterpreterGQLQuery.h>
#  include <Parsers/ASTFunction.h>
#  include <Parsers/ASTIdentifier.h>
#  include <Parsers/ASTLiteral.h>
#  include <Parsers/graph/GraphAST.h>
#  include <Parsers/graph/ParserGQLQuery.h>
#  include <Processors/QueryPlan/AggregatingStep.h>
#  include <Processors/QueryPlan/DistinctStep.h>
#  include <Processors/QueryPlan/ExpressionStep.h>
#  include <Processors/QueryPlan/FilterStep.h>
#  include <Processors/QueryPlan/Graph/MatchStep.h>
#  include <Processors/QueryPlan/LimitStep.h>
#  include <Processors/QueryPlan/QueryPlan.h>
#  include <Processors/QueryPlan/SortingStep.h>
#  include <Processors/Sources/Graph/MatchSource.h>

#  include <vector>

using namespace DB;

namespace GAST = DB::OPENGQL::AST;

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace
{

class TestMatchSourceReader final : public Graph::IMatchSourceReader
{
public:
    Chunk generate() override { return {}; }
};

class TestMatchSourceFactory final : public Graph::IMatchSourceFactory
{
public:
    std::unique_ptr<Graph::IMatchSourceReader> createReader(SharedHeader, const Graph::MatchSpec &) const override
    {
        return std::make_unique<TestMatchSourceReader>();
    }
};

GQL::PlanEnvironment makePlanEnvironment(Graph::MatchSourceFactoryPtr factory)
{
    GQL::PlanEnvironment environment;
    environment.match_source_factory = std::move(factory);
    return environment;
}

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

GQL::PlanScope buildScopeWithPlanBuilder(const std::string & query)
{
    const auto ast = parseGQL(query);
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "test query must parse to GQLSingleQuery");

    QueryPlan plan;
    GQL::PlanBuilder builder(getInterpreterContext());
    builder.buildSingleQuery(plan, *single_query);
    return builder.getScope();
}

std::vector<GQL::PlanBinding> buildScopeBindingsWithPlanBuilder(const std::string & query)
{
    return buildScopeWithPlanBuilder(query).getBindings();
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

void collectMatchSteps(const QueryPlan::Node * node, std::vector<const Graph::MatchStep *> & steps)
{
    if (!node)
        return;

    if (const auto * match_step = dynamic_cast<const Graph::MatchStep *>(node->step.get()))
        steps.push_back(match_step);

    for (const auto & child : node->children)
        collectMatchSteps(child, steps);
}

std::vector<const Graph::MatchStep *> collectMatchSteps(const QueryPlan & plan)
{
    std::vector<const Graph::MatchStep *> steps;
    collectMatchSteps(plan.getRootNode(), steps);
    return steps;
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

TEST(GQLInterpreter, MatchStepCarriesReusableSourceFactory)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();
    Graph::MatchStep step(Graph::MatchSpec{}, factory);

    EXPECT_EQ(step.getSourceFactory(), factory);

    const auto cloned_step = step.clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    EXPECT_EQ(cloned_match_step->getSourceFactory(), factory);
}

TEST(GQLInterpreter, MatchSourceAcceptsNullReaderAsEmptySource)
{
    auto source = std::make_shared<Graph::MatchSource>(std::make_shared<const Block>(), Graph::MatchSpec{}, nullptr);

    EXPECT_EQ(source->getName(), "GraphMatchSource");
}

TEST(GQLInterpreter, PlanBuilderConstructorSeedsMatchSourceFactory)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();
    const auto ast = parseGQL("MATCH (n) RETURN n");
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    ASSERT_NE(single_query, nullptr);

    QueryPlan plan;
    GQL::PlanBuilder(getInterpreterContext(), makePlanEnvironment(factory)).buildSingleQuery(plan, *single_query);

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    EXPECT_EQ(match_step->getSourceFactory(), factory);
}

TEST(GQLInterpreter, InterpreterConstructorSeedsMatchSourceFactory)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();
    InterpreterGQLQuery interpreter(parseGQL("MATCH (n) RETURN n"), getInterpreterContext(), makePlanEnvironment(factory));

    QueryPlan plan;
    interpreter.buildQueryPlan(plan);

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    EXPECT_EQ(match_step->getSourceFactory(), factory);
}

TEST(GQLInterpreter, InterpreterMatchSourceFactoryFlowsIntoCombinedChildren)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();
    InterpreterGQLQuery interpreter(
        parseGQL("MATCH (n) RETURN n UNION ALL MATCH (n) RETURN n"),
        getInterpreterContext(),
        makePlanEnvironment(factory));

    QueryPlan plan;
    interpreter.buildQueryPlan(plan);

    const auto match_steps = collectMatchSteps(plan);
    ASSERT_EQ(match_steps.size(), 2u);
    EXPECT_EQ(match_steps[0]->getSourceFactory(), factory);
    EXPECT_EQ(match_steps[1]->getSourceFactory(), factory);
}

TEST(GQLInterpreter, PlanEnvironmentMatchSourceFactoryFlowsIntoMatchStep)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();

    const auto ast = parseGQL("MATCH (n) RETURN n");
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    ASSERT_NE(single_query, nullptr);

    QueryPlan plan;
    GQL::PlanBuilder(getInterpreterContext(), GQL::PlanScope{}, makePlanEnvironment(factory)).buildSingleQuery(plan, *single_query);

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    EXPECT_EQ(match_step->getSourceFactory(), factory);
}

TEST(GQLInterpreter, SubqueryInheritsPlanEnvironmentMatchSourceFactory)
{
    auto factory = std::make_shared<TestMatchSourceFactory>();

    const auto ast = parseGQL("SELECT n FROM { MATCH (n) RETURN n }");
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    ASSERT_NE(single_query, nullptr);

    QueryPlan plan;
    GQL::PlanBuilder(getInterpreterContext(), GQL::PlanScope{}, makePlanEnvironment(factory)).buildSingleQuery(plan, *single_query);

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);
    EXPECT_EQ(match_step->getSourceFactory(), factory);
}

TEST(GQLInterpreter, UnionAllBuildsRootUnionPlan)
{
    const auto plan = buildPlan("RETURN 1 AS v UNION ALL RETURN 2 AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Union"}));
}

TEST(GQLInterpreter, UnionDistinctBuildsUnionThenDistinctPlan)
{
    const auto plan = buildPlan("RETURN 1 AS v UNION RETURN 1 AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Distinct", "Union"}));
}

TEST(GQLInterpreter, ExceptDistinctBuildsIntersectOrExceptPlan)
{
    const auto plan = buildPlan("RETURN 1 AS v EXCEPT RETURN 2 AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Distinct", "IntersectOrExcept"}));
}

TEST(GQLInterpreter, IntersectDistinctBuildsIntersectOrExceptPlan)
{
    const auto plan = buildPlan("RETURN 1 AS v INTERSECT RETURN 1 AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Distinct", "IntersectOrExcept"}));
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

TEST(GQLInterpreter, UseClauseSetsReusablePlanScopeGraph)
{
    const auto scope = buildScopeWithPlanBuilder("USE g RETURN 1 AS x");

    const auto & active_graph = scope.getActiveGraph();
    ASSERT_TRUE(active_graph);

    const auto * graph = active_graph->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "g");

    ASSERT_EQ(scope.getBindings().size(), 1u);
    EXPECT_EQ(scope.getBindings().front().name, "x");
}

TEST(GQLInterpreter, UseClauseFlowsIntoGraphMatchSpec)
{
    const auto plan = buildPlanWithPlanBuilder("USE g MATCH (n) RETURN n");

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);

    const auto & graph_reference = match_step->getMatchSpec().graph_reference;
    ASSERT_TRUE(graph_reference);

    const auto * graph = graph_reference->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "g");

    const auto cloned_step = match_step->clone();
    const auto * cloned_match_step = dynamic_cast<const Graph::MatchStep *>(cloned_step.get());
    ASSERT_NE(cloned_match_step, nullptr);
    EXPECT_NE(cloned_match_step->getMatchSpec().graph_reference.get(), graph_reference.get());
}

TEST(GQLInterpreter, SelectGraphMatchSourceFlowsIntoGraphMatchSpec)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT n FROM g MATCH (n)");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);

    const auto & graph_reference = match_step->getMatchSpec().graph_reference;
    ASSERT_TRUE(graph_reference);

    const auto * graph = graph_reference->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "g");
}

TEST(GQLInterpreter, SelectGraphMatchSourceDoesNotLeakUseScopeGraph)
{
    const auto scope = buildScopeWithPlanBuilder("USE outer_graph SELECT n FROM inner_graph MATCH (n)");

    const auto & active_graph = scope.getActiveGraph();
    ASSERT_TRUE(active_graph);

    const auto * graph = active_graph->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "outer_graph");

    ASSERT_EQ(scope.getBindings().size(), 1u);
    EXPECT_EQ(scope.getBindings().front().name, "n");
}

TEST(GQLInterpreter, SelectGraphSubquerySourceFlowsIntoNestedMatchSpec)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT n FROM g { MATCH (n) RETURN n }");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);

    const auto & graph_reference = match_step->getMatchSpec().graph_reference;
    ASSERT_TRUE(graph_reference);

    const auto * graph = graph_reference->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "g");
}

TEST(GQLInterpreter, UseClauseFlowsIntoInlineCallSubqueryMatchSpec)
{
    const auto plan = buildPlanWithPlanBuilder("USE g CALL { MATCH (n) RETURN n } RETURN n");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "GraphMatch"}));

    const auto * match_step = leafMatchStep(plan);
    ASSERT_NE(match_step, nullptr);

    const auto & graph_reference = match_step->getMatchSpec().graph_reference;
    ASSERT_TRUE(graph_reference);

    const auto * graph = graph_reference->as<GAST::GQLGraphExpression>();
    ASSERT_NE(graph, nullptr);
    EXPECT_EQ(graph->text, "g");
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

TEST(GQLInterpreter, SelectWithoutMatchUsesReusableProjectionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT 1 AS one");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "one");
}

TEST(GQLInterpreter, SelectFromSubqueryUsesReusableSourceLowering)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT a FROM { RETURN 1 AS a }");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
}

TEST(GQLInterpreter, InlineCallUsesReusableSourceLowering)
{
    const auto plan = buildPlanWithPlanBuilder("CALL { RETURN 1 AS a } RETURN a");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
}

TEST(GQLInterpreter, InlineCallEmptyVariableScopeUsesReusableSourceLowering)
{
    const auto plan = buildPlanWithPlanBuilder("CALL () { RETURN 1 AS a } RETURN a");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
}

TEST(GQLInterpreter, InlineCallUnavailableVariableScopeThrows)
{
    try
    {
        (void)buildPlanWithPlanBuilder("CALL (a) { RETURN a } RETURN a");
        FAIL() << "Expected missing inline CALL variable import to be rejected";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find("is not available"), String::npos);
    }
}

TEST(GQLInterpreter, InlineCallImportsExpressionBindingFromOuterScope)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT x FROM { VALUE x = 1 CALL (x) { RETURN x } RETURN x }");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Expression", "Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
}

TEST(GQLInterpreter, InlineCallValueBindingSeedsNestedReturn)
{
    const auto plan = buildPlanWithPlanBuilder("CALL { VALUE x = 1 RETURN x } RETURN x");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
}

TEST(GQLInterpreter, SubqueryValueBindingSurvivesNestedMatchSourceUntilProjection)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT n, x FROM { VALUE x = 1 MATCH (n) RETURN n, x }");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "GraphMatch"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 2u);
    EXPECT_EQ(header.getByPosition(0).name, "n");
    EXPECT_EQ(header.getByPosition(1).name, "x");
}

TEST(GQLInterpreter, SubqueryValueBindingDoesNotLeakWhenNotReturned)
{
    const auto bindings = buildScopeBindingsWithPlanBuilder("CALL { VALUE x = 1 RETURN 1 AS y } RETURN y");

    ASSERT_EQ(bindings.size(), 1u);
    EXPECT_EQ(bindings.front().name, "y");
}

TEST(GQLInterpreter, TypedSubqueryValueBindingUsesReusableTypeCastLowering)
{
    const auto plan = buildPlanWithPlanBuilder("CALL { VALUE x INT32 = 1 RETURN x } RETURN x");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
    EXPECT_EQ(header.getByPosition(0).type->getName(), "Int32");
}

TEST(GQLInterpreter, SelectAllFromSubqueryKeepsSourceHeader)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT * FROM { RETURN 1 AS a }");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
}

TEST(GQLInterpreter, SelectFromSubqueryReusesWhereProjectionAndPageLowering)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT a FROM { RETURN 1 AS a } WHERE a = 1 ORDER BY a LIMIT 1");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Limit", "Sorting", "Expression", "Filter", "Expression", "ReadFromPreparedSource"}));
}

TEST(GQLInterpreter, ReturnDistinctUsesReusableDistinctLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN DISTINCT 1 AS one");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Distinct", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * distinct = dynamic_cast<const DistinctStep *>(root->step.get());
    ASSERT_NE(distinct, nullptr);
    EXPECT_EQ(distinct->getColumnNames(), (Names{"one"}));
}

TEST(GQLInterpreter, SelectDistinctAllFromSubqueryUsesReusableDistinctLowering)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT DISTINCT * FROM { RETURN 1 AS a }");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Distinct", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * distinct = dynamic_cast<const DistinctStep *>(root->step.get());
    ASSERT_NE(distinct, nullptr);
    EXPECT_EQ(distinct->getColumnNames(), (Names{"a"}));
}

TEST(GQLInterpreter, LetWithoutMatchStartsReusableScalarPipeline)
{
    const auto plan = buildPlanWithPlanBuilder("LET x = 1 RETURN x");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
}

TEST(GQLInterpreter, LetAssignmentsCanReferenceEarlierBindings)
{
    const auto plan = buildPlanWithPlanBuilder("LET x = 1, y = x + 1 RETURN y");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "y");
}

TEST(GQLInterpreter, TypedLetValueUsesReusableTypeCastLowering)
{
    const auto plan = buildPlanWithPlanBuilder("LET VALUE y INT = 1 RETURN y");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "y");
    EXPECT_EQ(header.getByPosition(0).type->getName(), "Int64");
}

TEST(GQLInterpreter, ForWithoutMatchUsesReusableArrayJoinTransform)
{
    const auto plan = buildPlanWithPlanBuilder("FOR x IN [1, 2] RETURN x");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
}

TEST(GQLInterpreter, ForOffsetUsesAlignedArrayJoinTransform)
{
    const auto plan = buildPlanWithPlanBuilder("FOR x IN [1, 2] WITH OFFSET i RETURN x, i");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 2u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
    EXPECT_EQ(header.getByPosition(1).name, "i");
}

TEST(GQLInterpreter, ForOrdinalityUsesAlignedArrayJoinTransform)
{
    const auto plan = buildPlanWithPlanBuilder("FOR x IN [1, 2] WITH ORDINALITY ord RETURN x, ord");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 2u);
    EXPECT_EQ(header.getByPosition(0).name, "x");
    EXPECT_EQ(header.getByPosition(1).name, "ord");
}

TEST(GQLInterpreter, MatchFinishUsesReusableTerminalProjection)
{
    const auto plan = buildPlanWithPlanBuilder("MATCH (n) FINISH");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "GraphMatch"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    EXPECT_EQ(header.columns(), 0u);
}

TEST(GQLInterpreter, UseFinishStartsReusableScalarPipeline)
{
    const auto plan = buildPlanWithPlanBuilder("USE g FINISH");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    EXPECT_EQ(header.columns(), 0u);
}

TEST(GQLInterpreter, ReturnGroupByUsesReusableAggregationLowering)
{
    const auto plan = buildPlanWithPlanBuilder("FOR x IN [1, 1, 2] RETURN x, COUNT(*) AS c GROUP BY x");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Expression", "Aggregating", "Expression", "Expression", "ReadFromPreparedSource"}));
}

TEST(GQLInterpreter, SelectHavingUsesReusablePredicateLowering)
{
    const auto plan = buildPlanWithPlanBuilder("SELECT SUM(x) AS s FROM { FOR x IN [1, 2] RETURN x } HAVING s > 1");

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    ASSERT_NE(dynamic_cast<const ExpressionStep *>(root->step.get()), nullptr);
    ASSERT_EQ(root->children.size(), 1u);

    const auto * filter = dynamic_cast<const FilterStep *>(root->children.front()->step.get());
    ASSERT_NE(filter, nullptr);
    ASSERT_EQ(root->children.front()->children.size(), 1u);
    EXPECT_NE(dynamic_cast<const AggregatingStep *>(root->children.front()->children.front()->step.get()), nullptr);
}

TEST(GQLInterpreter, NativeClickHouseAggregateFunctionUsesReusableAggregationDetection)
{
    ASTs items;
    items.push_back(make_intrusive<GAST::GQLAliasedItem>(
        makeASTFunction("sum", make_intrusive<ASTIdentifier>("x")),
        "s"));

    EXPECT_TRUE(GQL::hasAggregateProjectionItems(items));
}

TEST(GQLInterpreter, NativeClickHouseGroupByIdentifierUsesReusableAggregationDetection)
{
    auto group_by = make_intrusive<GAST::GQLGroupByClause>();
    group_by->items.push_back(make_intrusive<ASTIdentifier>("x"));
    group_by->children.push_back(group_by->items.back());

    const auto keys = GQL::AggregationLoweringDetail::extractGroupByKeys(group_by.get());

    EXPECT_EQ(keys, Names{"x"});
}

TEST(GQLInterpreter, NativeClickHouseNonCountAggregateRequiresArguments)
{
    auto aggregate = makeASTFunction("sum");

    EXPECT_THROW(
        GQL::AggregationLoweringDetail::getAggregateFunctionInfo(*aggregate),
        DB::Exception);
}

TEST(GQLInterpreter, LetAfterMatchReusesPipelineTransform)
{
    const auto plan = buildPlanWithPlanBuilder("MATCH (n) LET x = n RETURN x");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "Expression", "GraphMatch"}));
}

TEST(GQLInterpreter, ReturnOffsetLimitUsesReusablePageLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN 1 AS one OFFSET 1 LIMIT 5");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Limit", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * limit = dynamic_cast<const LimitStep *>(root->step.get());
    ASSERT_NE(limit, nullptr);
    EXPECT_EQ(limit->getLimit(), 5u);
}

TEST(GQLInterpreter, ReturnOrderByUsesReusablePageLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN 1 AS a ORDER BY a DESC NULLS LAST LIMIT 5");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Limit", "Sorting", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->children.size(), 1u);
    const auto * sorting = dynamic_cast<const SortingStep *>(root->children.front()->step.get());
    ASSERT_NE(sorting, nullptr);

    const auto & description = sorting->getSortDescription();
    ASSERT_EQ(description.size(), 1u);
    EXPECT_EQ(description[0].column_name, "a");
    EXPECT_EQ(description[0].direction, -1);
    EXPECT_EQ(description[0].nulls_direction, -1);
}

TEST(GQLInterpreter, ReturnOrderByExpressionUsesHiddenSortColumn)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN 1 AS a ORDER BY a + 1 LIMIT 5");

    EXPECT_EQ(
        linearStepNames(plan),
        (std::vector<String>{"Limit", "Expression", "Sorting", "Expression", "Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto & header = *root->step->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "a");
}

TEST(GQLInterpreter, ScalarFunctionUsesReusableExpressionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN ABS(-1) AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "v");
}

TEST(GQLInterpreter, CastUsesReusableExpressionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN CAST(1 AS INT32) AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "v");
    EXPECT_EQ(header.getByPosition(0).type->getName(), "Int32");
}

TEST(GQLInterpreter, CaseUsesGenericExpressionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN CASE WHEN TRUE THEN 1 ELSE 0 END AS v");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "v");
}

TEST(GQLInterpreter, ListConstructorUsesGenericExpressionLowering)
{
    const auto plan = buildPlanWithPlanBuilder("RETURN [1, 2] AS xs");

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Expression", "ReadFromPreparedSource"}));

    const auto * root = plan.getRootNode();
    ASSERT_NE(root, nullptr);
    const auto * expression = dynamic_cast<const ExpressionStep *>(root->step.get());
    ASSERT_NE(expression, nullptr);

    const auto & header = *expression->getOutputHeader();
    ASSERT_EQ(header.columns(), 1u);
    EXPECT_EQ(header.getByPosition(0).name, "xs");
    EXPECT_EQ(header.getByPosition(0).type->getName(), "Array(UInt64)");
}

TEST(GQLInterpreter, NativeClickHousePagingExpressionsUseReusableLowering)
{
    const auto ast = parseGQL("RETURN 1 AS a");
    const auto * single_query = ast->as<GAST::GQLSingleQuery>();
    ASSERT_NE(single_query, nullptr);

    QueryPlan plan;
    GQL::PlanBuilder builder(getInterpreterContext());
    builder.buildSingleQuery(plan, *single_query);
    auto scope = builder.getScope();

    auto order_by_item = make_intrusive<GAST::GQLOrderByItem>(make_intrusive<ASTIdentifier>("a"), false);
    auto order_by = make_intrusive<GAST::GQLOrderByClause>(ASTs{order_by_item});
    auto page = make_intrusive<GAST::GQLPageClause>();
    page->order_by = order_by;
    page->limit = make_intrusive<ASTLiteral>(Field(UInt64(1)));
    page->children.push_back(page->order_by);
    page->children.push_back(page->limit);

    GQL::lowerPageClause(plan, *page, getInterpreterContext(), scope);

    EXPECT_EQ(linearStepNames(plan), (std::vector<String>{"Limit", "Sorting", "Expression", "ReadFromPreparedSource"}));
}

TEST(GQLInterpreter, NativeClickHouseLiteralUsesReusableExpressionLowering)
{
    ActionsDAG dag;
    const auto literal = make_intrusive<ASTLiteral>(Field(UInt64(7)));

    const auto & node = GQL::lowerExpression(*literal, dag, getInterpreterContext());

    EXPECT_EQ(node.result_type->getName(), "UInt64");
    ASSERT_TRUE(node.column);
}

TEST(GQLInterpreter, NativeClickHouseIdentifierUsesReusableExpressionLowering)
{
    NamesAndTypesList inputs;
    inputs.emplace_back("x", std::make_shared<DataTypeUInt64>());
    ActionsDAG dag(inputs);
    const auto identifier = make_intrusive<ASTIdentifier>("x");

    const auto & node = GQL::lowerExpression(*identifier, dag, getInterpreterContext());

    EXPECT_EQ(node.result_name, "x");
    EXPECT_EQ(node.result_type->getName(), "UInt64");
}

TEST(GQLInterpreter, NativeClickHouseFunctionUsesReusableExpressionLowering)
{
    NamesAndTypesList inputs;
    inputs.emplace_back("x", std::make_shared<DataTypeUInt64>());
    ActionsDAG dag(inputs);
    auto function = makeASTFunction(
        "plus",
        make_intrusive<ASTIdentifier>("x"),
        make_intrusive<ASTLiteral>(Field(UInt64(1))));

    const auto & node = GQL::lowerExpression(*function, dag, getInterpreterContext());

    EXPECT_EQ(node.result_type->getName(), "UInt64");
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
