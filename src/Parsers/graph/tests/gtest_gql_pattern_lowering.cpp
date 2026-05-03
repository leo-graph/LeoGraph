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
    EXPECT_EQ(binding.edges.front().quantifier, nullptr);
}

TEST(GQLPatternLowering, MatchPathVariableLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH p = (a)-[r]->(b) RETURN a");

    const auto binding = lowerOnlyPathPattern(ast);

    EXPECT_EQ(binding.variable, "p");
    ASSERT_EQ(binding.nodes.size(), 2u);
    ASSERT_EQ(binding.edges.size(), 1u);

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));

    ASSERT_EQ(spec.paths.size(), 1u);
    EXPECT_EQ(spec.paths.front().variable, "p");
    ASSERT_EQ(spec.paths.front().nodes.size(), 2u);
    ASSERT_EQ(spec.paths.front().edges.size(), 1u);
}

TEST(GQLPatternLowering, MatchEdgeQuantifierLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r]->{2,5}(b) RETURN r");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_EQ(binding.edges.size(), 1u);
    ASSERT_NE(binding.edges.front().quantifier, nullptr);
    EXPECT_EQ(binding.edges.front().quantifier->kind, GAST::GQLQuantifier::Kind::Range);
    EXPECT_EQ(binding.edges.front().quantifier->lower, "2");
    EXPECT_EQ(binding.edges.front().quantifier->upper, "5");

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));

    ASSERT_EQ(spec.paths.size(), 1u);
    ASSERT_EQ(spec.paths.front().edges.size(), 1u);
    ASSERT_NE(spec.paths.front().edges.front().quantifier, nullptr);
    const auto * quantifier = spec.paths.front().edges.front().quantifier->as<GAST::GQLQuantifier>();
    ASSERT_NE(quantifier, nullptr);
    EXPECT_EQ(quantifier->kind, GAST::GQLQuantifier::Kind::Range);
    EXPECT_EQ(quantifier->lower, "2");
    EXPECT_EQ(quantifier->upper, "5");
}

TEST(GQLPatternLowering, MatchPathPrefixLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH ANY SHORTEST (a)-[r]->(b) RETURN a");

    const auto binding = lowerOnlyPathPattern(ast);

    ASSERT_NE(binding.prefix, nullptr);
    const auto * lowered_prefix = binding.prefix->as<GAST::GQLPathSearchPrefix>();
    ASSERT_NE(lowered_prefix, nullptr);
    EXPECT_EQ(lowered_prefix->search_kind, GAST::PathSearchKind::AnyShortest);

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));

    ASSERT_EQ(spec.paths.size(), 1u);
    ASSERT_NE(spec.paths.front().prefix, nullptr);
    EXPECT_NE(spec.paths.front().prefix.get(), binding.prefix);
    const auto * spec_prefix = spec.paths.front().prefix->as<GAST::GQLPathSearchPrefix>();
    ASSERT_NE(spec_prefix, nullptr);
    EXPECT_EQ(spec_prefix->search_kind, GAST::PathSearchKind::AnyShortest);
}

TEST(GQLPatternLowering, MatchKeepClauseLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r]->(b) KEEP ANY 2 PATHS RETURN a");

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto lowered = GQL::lowerMatchClause(*match);

    ASSERT_TRUE(lowered.has_keep_clause);
    ASSERT_NE(lowered.keep_clause, nullptr);
    const auto spec = GQL::makeMatchSpec(lowered);

    ASSERT_TRUE(spec.has_keep_clause);
    ASSERT_NE(spec.keep_clause, nullptr);
    EXPECT_NE(spec.keep_clause.get(), lowered.keep_clause);
    const auto * keep = spec.keep_clause->as<GAST::GQLKeepClause>();
    ASSERT_NE(keep, nullptr);
    ASSERT_NE(keep->path_prefix, nullptr);
    const auto * prefix = keep->path_prefix->as<GAST::GQLPathSearchPrefix>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->search_kind, GAST::PathSearchKind::Any);
    EXPECT_EQ(prefix->count_kind, GAST::CountKind::Paths);
}

TEST(GQLPatternLowering, MatchWhereClauseLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r]->(b) WHERE a IS NOT NULL RETURN a");

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto lowered = GQL::lowerMatchClause(*match);
    ASSERT_NE(lowered.where, nullptr);

    const auto spec = GQL::makeMatchSpec(lowered);
    ASSERT_NE(spec.where_clause, nullptr);
    EXPECT_NE(spec.where_clause.get(), lowered.where);
    const auto * where = spec.where_clause->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);
    EXPECT_EQ(where->type, GAST::GQLWhereClause::Type::Where);
    ASSERT_NE(where->expression, nullptr);
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
    EXPECT_EQ(spec.match_mode, Graph::MatchMode::None);
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

TEST(GQLPatternLowering, MatchModeLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH DIFFERENT EDGES (a)-[r]->(b) RETURN a");

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));

    EXPECT_TRUE(spec.has_match_mode);
    EXPECT_EQ(spec.match_mode, Graph::MatchMode::DifferentEdges);
}

TEST(GQLPatternLowering, MatchYieldItemsLowerToSpec)
{
    auto ast = parseGQLOrThrow("MATCH (a)-[r]->(b) YIELD r RETURN r");

    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);
    const auto spec = GQL::makeMatchSpec(GQL::lowerMatchClause(*match));

    EXPECT_TRUE(spec.has_yield_items);
    ASSERT_EQ(spec.yield_items.size(), 1u);
    ASSERT_EQ(spec.yield_variables.size(), 1u);
    EXPECT_EQ(spec.yield_variables.front(), "r");

    const auto * yield_expr = spec.yield_items.front()->as<GAST::GQLExpr>();
    ASSERT_NE(yield_expr, nullptr);
    EXPECT_EQ(yield_expr->kind, GAST::GQLExpr::Kind::Identifier);
    EXPECT_EQ(yield_expr->text, "r");
}

TEST(GQLPatternLowering, MatchSequenceLowersToSpec)
{
    auto ast = parseGQLOrThrow("MATCH (a) MATCH (b) RETURN a, b");
    const auto * query = getSingleQuery(ast);
    ASSERT_NE(query, nullptr);

    std::vector<GQL::MatchPlan> match_plans;
    for (const auto & clause : query->clauses)
    {
        if (const auto * match = clause->as<GAST::GQLMatchClause>())
            match_plans.push_back(GQL::lowerMatchClause(*match));
    }

    ASSERT_EQ(match_plans.size(), 2u);
    const auto spec = GQL::makeMatchSpec(match_plans);

    ASSERT_EQ(spec.clauses.size(), 2u);
    ASSERT_EQ(spec.paths.size(), 2u);
    ASSERT_EQ(spec.clauses[0].paths.size(), 1u);
    ASSERT_EQ(spec.clauses[1].paths.size(), 1u);
    EXPECT_EQ(spec.clauses[0].paths.front().nodes.front().variable, "a");
    EXPECT_EQ(spec.clauses[1].paths.front().nodes.front().variable, "b");
}

TEST(GQLPatternLowering, OptionalMatchOperandBlockLowersToSpec)
{
    auto ast = parseGQLOrThrow("OPTIONAL { MATCH (a) MATCH (a)-[r]->(b) } RETURN *");
    const auto * match = getMatchClause(ast);
    ASSERT_NE(match, nullptr);

    const auto lowered = GQL::lowerMatchClause(*match);
    EXPECT_TRUE(lowered.optional);
    EXPECT_TRUE(lowered.has_optional_operand_block);
    ASSERT_NE(lowered.optional_operand_block, nullptr);

    const auto spec = GQL::makeMatchSpec(lowered);
    EXPECT_TRUE(spec.optional);
    EXPECT_TRUE(spec.has_optional_operand_block);
    ASSERT_NE(spec.optional_operand_block, nullptr);
    EXPECT_NE(spec.optional_operand_block.get(), lowered.optional_operand_block);

    const auto * block = spec.optional_operand_block->as<GAST::GQLMatchStatementBlock>();
    ASSERT_NE(block, nullptr);
    EXPECT_FALSE(block->parenthesized);
    EXPECT_EQ(block->matches.size(), 2u);
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
    expectUnsupportedPathPattern("MATCH (a)-[r]->(b) | (c)-[s]->(d) RETURN a", "multiple GQLPathTerms");
}

#endif
