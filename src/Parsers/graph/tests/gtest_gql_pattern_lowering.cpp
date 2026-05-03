#include "config.h"

#if USE_GQL_GRAMMAR

#  include <gtest/gtest.h>

#  include <Common/Exception.h>
#  include <Interpreters/GQL/MatchPlanToSpec.h>
#  include <Interpreters/GQL/PatternLowering.h>
#  include <Parsers/graph/GQLParserUtils.h>
#  include <Parsers/graph/GraphAST.h>
#  include <Parsers/graph/ParserGQLQuery.h>
#  include <Processors/QueryPlan/Graph/MatchSpec.h>

#  include <string_view>

using namespace DB;

namespace GAST = DB::OPENGQL::AST;

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace
{

ASTPtr parseGQLOrThrow(std::string_view query)
{
    ParserGQLQuery parser;
    const String query_string(query.data(), query.size());
    return DB::parseGQLQuery(parser, query_string, 0, 0, 0);
}

const GAST::GQLSingleQuery * getSingleQuery(const ASTPtr & ast)
{
    const auto * query = ast->as<GAST::GQLSingleQuery>();
    EXPECT_NE(query, nullptr);
    return query;
}

const GAST::GQLMatchClause * getMatchClause(const ASTPtr & ast)
{
    const auto * query = getSingleQuery(ast);
    if (!query || query->clauses.empty())
        return nullptr;

    const auto * match = query->clauses.front()->as<GAST::GQLMatchClause>();
    EXPECT_NE(match, nullptr);
    return match;
}

const GAST::GQLPathPattern * getOnlyPathPattern(const ASTPtr & ast)
{
    const auto * match = getMatchClause(ast);
    if (!match || match->path_patterns.size() != 1)
        return nullptr;

    const auto * path = match->path_patterns.front()->as<GAST::GQLPathPattern>();
    EXPECT_NE(path, nullptr);
    return path;
}

GQL::PathBinding lowerOnlyPathPattern(const ASTPtr & ast)
{
    const auto * path = getOnlyPathPattern(ast);
    EXPECT_NE(path, nullptr);
    return GQL::lowerPathPattern(*path);
}

void expectNameLabel(const GAST::GQLLabelExpression * label, const String & text)
{
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->kind, GAST::GQLLabelExpression::Kind::Name);
    EXPECT_EQ(label->text, text);
}

void expectUnsupportedPathPattern(std::string_view query, std::string_view expected_message)
{
    auto ast = parseGQLOrThrow(query);
    const auto * path = getOnlyPathPattern(ast);
    ASSERT_NE(path, nullptr);

    try
    {
        (void)GQL::lowerPathPattern(*path);
        FAIL() << "Expected unsupported GQL path pattern to throw";
    }
    catch (const Exception & e)
    {
        EXPECT_EQ(e.code(), ErrorCodes::NOT_IMPLEMENTED);
        EXPECT_NE(String(e.message()).find(String(expected_message.data(), expected_message.size())), String::npos);
    }
}

}

TEST(GQLPatternLowering, MatchNamedNodeLowersSingleNodeBinding)
{
    auto ast = parseGQLOrThrow("MATCH (n) RETURN n");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_EQ(binding.nodes.size(), 1u);
    EXPECT_EQ(binding.nodes.front().variable, "n");
    EXPECT_EQ(binding.nodes.front().label, nullptr);
    EXPECT_EQ(binding.nodes.front().properties, nullptr);
    EXPECT_EQ(binding.nodes.front().where, nullptr);
    EXPECT_TRUE(binding.edges.empty());
}

TEST(GQLPatternLowering, MatchAnonymousNodeLowersEmptyVariable)
{
    auto ast = parseGQLOrThrow("MATCH () RETURN *");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_EQ(binding.nodes.size(), 1u);
    EXPECT_EQ(binding.nodes.front().variable, "");
    EXPECT_TRUE(binding.edges.empty());
}

TEST(GQLPatternLowering, MatchNodeLabelAndPropertiesKeepRawPointers)
{
    auto ast = parseGQLOrThrow("MATCH (n:Person {name: 'x'}) RETURN n");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_EQ(binding.nodes.size(), 1u);
    EXPECT_EQ(binding.nodes.front().variable, "n");
    expectNameLabel(binding.nodes.front().label, "Person");
    EXPECT_NE(binding.nodes.front().properties, nullptr);
    EXPECT_EQ(binding.nodes.front().where, nullptr);
}

TEST(GQLPatternLowering, MatchOutgoingEdgeLowersEdgeBinding)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r:KNOWS]->(b) RETURN r");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_EQ(binding.nodes.size(), 2u);
    ASSERT_EQ(binding.edges.size(), 1u);
    EXPECT_EQ(binding.nodes[0].variable, "a");
    EXPECT_EQ(binding.nodes[1].variable, "b");
    EXPECT_EQ(binding.edges.front().variable, "r");
    EXPECT_EQ(binding.edges.front().direction, GQL::EdgeBinding::Direction::Outgoing);
    expectNameLabel(binding.edges.front().label, "KNOWS");
    EXPECT_EQ(binding.edges.front().properties, nullptr);
    EXPECT_EQ(binding.edges.front().where, nullptr);
}

TEST(GQLPatternLowering, MatchCompoundEdgeDirectionsLowerToSpec)
{
    {
        auto ast = parseGQLOrThrow("MATCH (a)<-[r]->(b) RETURN r");
        const auto binding = lowerOnlyPathPattern(ast);
        ASSERT_EQ(binding.edges.size(), 1u);
        EXPECT_EQ(binding.edges.front().direction, GQL::EdgeBinding::Direction::IncomingOrOutgoing);

        const auto * match = getMatchClause(ast);
        ASSERT_NE(match, nullptr);
        const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));
        ASSERT_EQ(spec.paths.size(), 1u);
        ASSERT_EQ(spec.paths.front().edges.size(), 1u);
        EXPECT_EQ(spec.paths.front().edges.front().direction, Graph::MatchEdgeDirection::IncomingOrOutgoing);
    }

    {
        auto ast = parseGQLOrThrow("MATCH (a)<~[r]~(b) RETURN r");
        const auto binding = lowerOnlyPathPattern(ast);
        ASSERT_EQ(binding.edges.size(), 1u);
        EXPECT_EQ(binding.edges.front().direction, GQL::EdgeBinding::Direction::IncomingOrUndirected);

        const auto * match = getMatchClause(ast);
        ASSERT_NE(match, nullptr);
        const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));
        ASSERT_EQ(spec.paths.size(), 1u);
        ASSERT_EQ(spec.paths.front().edges.size(), 1u);
        EXPECT_EQ(spec.paths.front().edges.front().direction, Graph::MatchEdgeDirection::IncomingOrUndirected);
    }

    {
        auto ast = parseGQLOrThrow("MATCH (a)~[r]~>(b) RETURN r");
        const auto binding = lowerOnlyPathPattern(ast);
        ASSERT_EQ(binding.edges.size(), 1u);
        EXPECT_EQ(binding.edges.front().direction, GQL::EdgeBinding::Direction::UndirectedOrOutgoing);

        const auto * match = getMatchClause(ast);
        ASSERT_NE(match, nullptr);
        const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));
        ASSERT_EQ(spec.paths.size(), 1u);
        ASSERT_EQ(spec.paths.front().edges.size(), 1u);
        EXPECT_EQ(spec.paths.front().edges.front().direction, Graph::MatchEdgeDirection::UndirectedOrOutgoing);
    }
}

TEST(GQLPatternLowering, MatchClauseLoweringPreservesPatternAndWhere)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r:KNOWS]->(b) WHERE a IS NOT NULL RETURN r");
    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);

    const auto lowered = GQL::lowerMatchClause(*match);

    EXPECT_FALSE(lowered.optional);
    EXPECT_EQ(lowered.paths.size(), 1u);
    ASSERT_EQ(lowered.paths.front().nodes.size(), 2u);
    ASSERT_EQ(lowered.paths.front().edges.size(), 1u);
    EXPECT_EQ(lowered.paths.front().nodes[0].variable, "a");
    EXPECT_EQ(lowered.paths.front().nodes[1].variable, "b");
    EXPECT_EQ(lowered.paths.front().edges.front().variable, "r");
    EXPECT_NE(lowered.paths.front().edges.front().label, nullptr);
    EXPECT_NE(lowered.where, nullptr);

    const auto spec = GQL::makeMatchSpec(lowered);

    EXPECT_FALSE(spec.optional);
    EXPECT_FALSE(spec.has_match_mode);
    ASSERT_EQ(spec.paths.size(), 1u);
    ASSERT_EQ(spec.paths.front().nodes.size(), 2u);
    ASSERT_EQ(spec.paths.front().edges.size(), 1u);
    EXPECT_EQ(spec.paths.front().nodes[0].variable, "a");
    EXPECT_EQ(spec.paths.front().nodes[1].variable, "b");
    EXPECT_EQ(spec.paths.front().edges.front().variable, "r");
    EXPECT_EQ(spec.paths.front().edges.front().direction, Graph::MatchEdgeDirection::Outgoing);
    EXPECT_NE(spec.paths.front().edges.front().label_expression, nullptr);
    EXPECT_EQ(spec.paths.front().edges.front().properties, nullptr);
    EXPECT_EQ(spec.paths.front().edges.front().predicate, nullptr);
}

TEST(GQLPatternLowering, MatchSpecPreservesNodePatternConstraintAst)
{
    auto ast = parseGQLOrThrow("MATCH (n:Person {name: 'x'} WHERE n IS NOT NULL) RETURN n");
    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);

    const auto lowered = GQL::lowerMatchClause(*match);
    const auto spec = GQL::makeMatchSpec(lowered);

    ASSERT_EQ(spec.paths.size(), 1u);
    ASSERT_EQ(spec.paths.front().nodes.size(), 1u);

    const auto & node = spec.paths.front().nodes.front();
    EXPECT_EQ(node.variable, "n");
    EXPECT_NE(node.label_expression, nullptr);
    EXPECT_NE(node.properties, nullptr);
    EXPECT_NE(node.predicate, nullptr);
}

TEST(GQLPatternLowering, UnsupportedPathShapesThrowNotImplemented)
{
    expectUnsupportedPathPattern("MATCH p = (a)-[r]->(b) RETURN p", "path variable");
    expectUnsupportedPathPattern("MATCH (a)-[r]->(b) | (c)-[s]->(d) RETURN a", "multiple GQLPathTerms");
    /// `path prefix` rejection (e.g. ANY SHORTEST) cannot be exercised through `parseGQLQuery`
    /// yet: the dialect parser does not produce a `GQLPathSearchPrefix` for those inputs.
    /// Re-enable this fixture once the parser covers the path-prefix grammar branch.
}

#endif
