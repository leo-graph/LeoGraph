#include "config.h"

#if USE_GQL_GRAMMAR

#  include <gtest/gtest.h>

#  include <Common/Exception.h>
#  include <Interpreters/GQL/PatternLowering.h>
#  include <Parsers/graph/GQLParserUtils.h>
#  include <Parsers/graph/GraphAST.h>
#  include <Parsers/graph/ParserGQLQuery.h>

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

TEST(GQLPatternLowering, UnsupportedPathShapesThrowNotImplemented)
{
    expectUnsupportedPathPattern("MATCH p = (a)-[r]->(b) RETURN p", "path variable");
    expectUnsupportedPathPattern("MATCH ANY SHORTEST (a)-[*]->(b) RETURN a", "path prefix");
    expectUnsupportedPathPattern("MATCH (a)-[r]->(b) | (c)-[s]->(d) RETURN a", "multiple GQLPathTerms");
}

#endif
