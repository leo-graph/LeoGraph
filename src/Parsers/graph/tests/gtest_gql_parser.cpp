#include "config.h"

#if USE_GQL_GRAMMAR

#    include <Common/Exception.h>
#    include <IO/WriteBufferFromString.h>
#    include <Parsers/ParserQuery.h>
#    include <Parsers/graph/GraphAST.h>
#    include <Parsers/graph/ParserGraphQuery.h>
#    include <Parsers/parseQuery.h>
#    include <gtest/gtest.h>

using namespace DB;

namespace GAST = DB::OPENGQL::AST;

namespace
{

ASTPtr parseGraphOrThrow(std::string_view query)
{
    return DB::parseQuery(query);
}

ASTPtr parseTopLevelOrThrow(const String & query)
{
    ParserQuery parser(query.data() + query.size());
    return DB::parseQuery(parser, query, 0, 0, 0);
}

String formatAST(const IAST & ast)
{
    WriteBufferFromOwnString out;
    IAST::FormatSettings settings(false);
    IAST::FormatState state;
    ast.format(out, settings, state, {});
    return out.str();
}

const GAST::GQLClausesQuery * getClausesQuery(const ASTPtr & ast)
{
    const auto * clauses = ast->as<GAST::GQLClausesQuery>();
    EXPECT_NE(clauses, nullptr);
    return clauses;
}

const GAST::GQLMatchClause * getMatchClause(const GAST::GQLClausesQuery & clauses, size_t index = 0)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * match = clauses.clauses[index]->as<GAST::GQLMatchClause>();
    EXPECT_NE(match, nullptr);
    return match;
}

const GAST::GQLProjectClause * getProjectClause(const GAST::GQLClausesQuery & clauses, size_t index = 1)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * project = clauses.clauses[index]->as<GAST::GQLProjectClause>();
    EXPECT_NE(project, nullptr);
    return project;
}

const GAST::GQLPathPattern * getOnlyPathPattern(const GAST::GQLMatchClause & match)
{
    if (match.path_patterns.size() != 1)
        return nullptr;

    const auto * path = match.path_patterns.front()->as<GAST::GQLPathPattern>();
    EXPECT_NE(path, nullptr);
    return path;
}

const GAST::GQLExpr * getExpr(const ASTPtr & ast)
{
    const auto * expression = ast->as<GAST::GQLExpr>();
    EXPECT_NE(expression, nullptr);
    return expression;
}

const GAST::GQLLabelExpression * getLabelExpr(const ASTPtr & ast)
{
    const auto * expression = ast->as<GAST::GQLLabelExpression>();
    EXPECT_NE(expression, nullptr);
    return expression;
}

const GAST::GQLQuantifier * getQuantifier(const ASTPtr & ast)
{
    const auto * quantifier = ast->as<GAST::GQLQuantifier>();
    EXPECT_NE(quantifier, nullptr);
    return quantifier;
}

void attachChildIfPresent(IAST & owner, const ASTPtr & child)
{
    if (child)
        owner.children.push_back(child);
}

}

TEST(GQLParser, SimpleMatchClause)
{
    auto ast = parseGraphOrThrow("MATCH (n:Person) RETURN n");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);
    EXPECT_FALSE(match->optional);

    const auto * path = getOnlyPathPattern(*match);
    ASSERT_NE(path, nullptr);
    ASSERT_EQ(path->elements.size(), 1);

    const auto * node = path->elements[0]->as<GAST::GQLNodePattern>();
    ASSERT_NE(node, nullptr);

    const auto * variable = getExpr(node->variable);
    ASSERT_NE(variable, nullptr);
    EXPECT_EQ(variable->kind, GAST::GQLExpr::Kind::Identifier);
    EXPECT_EQ(variable->text, "n");

    const auto * label = getLabelExpr(node->label_expression);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->kind, GAST::GQLLabelExpression::Kind::Name);
    EXPECT_EQ(label->text, "Person");
}

TEST(GQLParser, OptionalMatchIsAcceptedByTopLevelParser)
{
    auto ast = parseTopLevelOrThrow("OPTIONAL MATCH (a) RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * match = getMatchClause(*clauses, 0);
    ASSERT_NE(match, nullptr);
    EXPECT_TRUE(match->optional);

    const auto * project = getProjectClause(*clauses, 1);
    ASSERT_NE(project, nullptr);
    ASSERT_EQ(project->items.size(), 1);
}

TEST(GQLParser, FocusedUseGraphQueryPreservesUseClause)
{
    auto ast = parseGraphOrThrow("USE foo MATCH (a) RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 3);

    const auto * use_clause = getExpr(clauses->clauses[0]);
    ASSERT_NE(use_clause, nullptr);
    EXPECT_EQ(use_clause->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(use_clause->text, "USE foo");

    const auto * match = getMatchClause(*clauses, 1);
    const auto * project = getProjectClause(*clauses, 2);
    ASSERT_NE(match, nullptr);
    ASSERT_NE(project, nullptr);
}

TEST(GQLParser, FocusedNestedQueryIsFlattened)
{
    auto ast = parseGraphOrThrow("USE foo { MATCH (a) RETURN a }");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 3);

    const auto * use_clause = getExpr(clauses->clauses[0]);
    ASSERT_NE(use_clause, nullptr);
    EXPECT_EQ(use_clause->text, "USE foo");

    const auto * match = getMatchClause(*clauses, 1);
    const auto * project = getProjectClause(*clauses, 2);
    ASSERT_NE(match, nullptr);
    ASSERT_NE(project, nullptr);
}

TEST(GQLParser, SelectStatementBuildsTopLevelProject)
{
    auto ast = parseGraphOrThrow("SELECT DISTINCT a AS x FROM { MATCH (a) RETURN a } WHERE a IS NOT NULL GROUP BY a ORDER BY a DESC OFFSET 1 LIMIT 2");
    const auto * project = ast->as<GAST::GQLProjectClause>();
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->type, GAST::GQLProjectClause::Type::Select);
    EXPECT_TRUE(project->distinct);
    ASSERT_EQ(project->items.size(), 1);

    const auto * item = getExpr(project->items[0]);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->text, "a");
    EXPECT_EQ(item->tryGetAlias(), "x");

    const auto * source = getExpr(project->source);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->kind, GAST::GQLExpr::Kind::RawText);

    const auto * where = project->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);
    const auto * group_by = getExpr(project->group_by);
    ASSERT_NE(group_by, nullptr);
    EXPECT_EQ(group_by->text, "GROUP BY a");

    const auto * order_by = project->order_by->as<GAST::GQLOrderByClause>();
    ASSERT_NE(order_by, nullptr);
    const auto * offset = getExpr(project->offset);
    const auto * limit = getExpr(project->limit);
    ASSERT_NE(offset, nullptr);
    ASSERT_NE(limit, nullptr);
    EXPECT_EQ(offset->text, "1");
    EXPECT_EQ(limit->text, "2");
}

TEST(GQLParser, RightEdgeWithRangeQuantifier)
{
    auto ast = parseGraphOrThrow("MATCH (a)-[e:KNOWS]->{2,5}(b) RETURN e");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * path = getOnlyPathPattern(*match);
    ASSERT_NE(path, nullptr);
    ASSERT_EQ(path->elements.size(), 3);

    const auto * edge = path->elements[1]->as<GAST::GQLEdgePattern>();
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->direction, GAST::EdgeDirection::Right);

    const auto * variable = getExpr(edge->variable);
    ASSERT_NE(variable, nullptr);
    EXPECT_EQ(variable->text, "e");

    const auto * label = getLabelExpr(edge->label_expression);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text, "KNOWS");

    const auto * quantifier = getQuantifier(edge->quantifier);
    ASSERT_NE(quantifier, nullptr);
    EXPECT_EQ(quantifier->kind, GAST::GQLQuantifier::Kind::Range);
    EXPECT_EQ(quantifier->lower, "2");
    EXPECT_EQ(quantifier->upper, "5");
}

TEST(GQLParser, LabelConjunction)
{
    auto ast = parseGraphOrThrow("MATCH (n:Person&Employee) RETURN n");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * path = getOnlyPathPattern(*match);
    ASSERT_NE(path, nullptr);

    const auto * node = path->elements[0]->as<GAST::GQLNodePattern>();
    ASSERT_NE(node, nullptr);

    const auto * conjunction = getLabelExpr(node->label_expression);
    ASSERT_NE(conjunction, nullptr);
    EXPECT_EQ(conjunction->kind, GAST::GQLLabelExpression::Kind::Conjunction);
    ASSERT_EQ(conjunction->children.size(), 2);

    const auto * left = getLabelExpr(conjunction->children[0]);
    const auto * right = getLabelExpr(conjunction->children[1]);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->text, "Person");
    EXPECT_EQ(right->text, "Employee");
}

TEST(GQLParser, WhereExpressionKeepsStructure)
{
    auto ast = parseGraphOrThrow("MATCH (a:Person) WHERE a.age > 20 AND a.age < 40 RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * root = getExpr(where->expression);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->kind, GAST::GQLExpr::Kind::BinaryOp);
    EXPECT_EQ(root->text, "AND");
    ASSERT_EQ(root->children.size(), 2);

    const auto * left = getExpr(root->children[0]);
    const auto * right = getExpr(root->children[1]);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->text, ">");
    EXPECT_EQ(right->text, "<");
}

TEST(GQLParser, PropertyAccessExpression)
{
    auto ast = parseGraphOrThrow("MATCH (a) WHERE a.name = 'Bob' RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * equals = getExpr(where->expression);
    ASSERT_NE(equals, nullptr);
    EXPECT_EQ(equals->text, "=");
    ASSERT_EQ(equals->children.size(), 2);

    const auto * property = getExpr(equals->children[0]);
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property->kind, GAST::GQLExpr::Kind::Property);
    EXPECT_EQ(property->text, "name");
    ASSERT_EQ(property->children.size(), 1);

    const auto * identifier = getExpr(property->children[0]);
    ASSERT_NE(identifier, nullptr);
    EXPECT_EQ(identifier->kind, GAST::GQLExpr::Kind::Identifier);
    EXPECT_EQ(identifier->text, "a");

    const auto * literal = getExpr(equals->children[1]);
    ASSERT_NE(literal, nullptr);
    EXPECT_EQ(literal->kind, GAST::GQLExpr::Kind::Literal);
    EXPECT_EQ(literal->text, "'Bob'");
}

TEST(GQLParser, TruthValuePredicateKeepsStructure)
{
    auto ast = parseGraphOrThrow("MATCH (a) WHERE a IS NOT TRUE RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * predicate = getExpr(where->expression);
    ASSERT_NE(predicate, nullptr);
    EXPECT_EQ(predicate->kind, GAST::GQLExpr::Kind::BinaryOp);
    EXPECT_EQ(predicate->text, "IS NOT");

    const auto * left = getExpr(predicate->children[0]);
    const auto * right = getExpr(predicate->children[1]);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->text, "a");
    EXPECT_EQ(right->kind, GAST::GQLExpr::Kind::Literal);
    EXPECT_EQ(right->text, "TRUE");
}

TEST(GQLParser, NullPredicateKeepsStructure)
{
    auto ast = parseGraphOrThrow("MATCH (a) WHERE a.name IS NOT NULL RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * predicate = getExpr(where->expression);
    ASSERT_NE(predicate, nullptr);
    EXPECT_EQ(predicate->text, "IS NOT");

    const auto * property = getExpr(predicate->children[0]);
    const auto * null_literal = getExpr(predicate->children[1]);
    ASSERT_NE(property, nullptr);
    ASSERT_NE(null_literal, nullptr);
    EXPECT_EQ(property->kind, GAST::GQLExpr::Kind::Property);
    EXPECT_EQ(property->text, "name");
    EXPECT_EQ(null_literal->text, "NULL");
}

TEST(GQLParser, PropertyExistsPredicateBecomesFunctionCall)
{
    auto ast = parseGraphOrThrow("MATCH (a) WHERE PROPERTY_EXISTS(a, name) RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * function = getExpr(where->expression);
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(function->kind, GAST::GQLExpr::Kind::FunctionCall);
    EXPECT_EQ(function->text, "PROPERTY_EXISTS");
    ASSERT_EQ(function->children.size(), 2);

    const auto * element = getExpr(function->children[0]);
    const auto * property = getExpr(function->children[1]);
    ASSERT_NE(element, nullptr);
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(element->text, "a");
    EXPECT_EQ(property->text, "name");
}

TEST(GQLParser, AllDifferentPredicateBecomesFunctionCall)
{
    auto ast = parseGraphOrThrow("MATCH (a)-[e]->(b) WHERE ALL_DIFFERENT(a, b, e) RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * match = getMatchClause(*clauses);
    ASSERT_NE(match, nullptr);

    const auto * where = match->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);

    const auto * function = getExpr(where->expression);
    ASSERT_NE(function, nullptr);
    EXPECT_EQ(function->kind, GAST::GQLExpr::Kind::FunctionCall);
    EXPECT_EQ(function->text, "ALL_DIFFERENT");
    ASSERT_EQ(function->children.size(), 3);
}

TEST(GQLParser, ReturnClauseKeepsAliasAndTail)
{
    auto ast = parseGraphOrThrow("MATCH (a:Person) RETURN a.name AS name, a.age ORDER BY a.age DESC OFFSET 2 LIMIT 5");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * project = getProjectClause(*clauses);
    ASSERT_NE(project, nullptr);
    EXPECT_FALSE(project->distinct);
    ASSERT_EQ(project->items.size(), 2);

    const auto * aliased = getExpr(project->items[0]);
    ASSERT_NE(aliased, nullptr);
    EXPECT_EQ(aliased->tryGetAlias(), "name");

    const auto * order_by = project->order_by->as<GAST::GQLOrderByClause>();
    ASSERT_NE(order_by, nullptr);
    ASSERT_EQ(order_by->items.size(), 1);

    const auto * order_item = order_by->items[0]->as<GAST::GQLOrderByItem>();
    ASSERT_NE(order_item, nullptr);
    EXPECT_TRUE(order_item->descending);

    const auto * offset = getExpr(project->offset);
    const auto * limit = getExpr(project->limit);
    ASSERT_NE(offset, nullptr);
    ASSERT_NE(limit, nullptr);
    EXPECT_EQ(offset->kind, GAST::GQLExpr::Kind::Literal);
    EXPECT_EQ(limit->kind, GAST::GQLExpr::Kind::Literal);
    EXPECT_EQ(offset->text, "2");
    EXPECT_EQ(limit->text, "5");
}

TEST(GQLParser, CompositeUnionQuery)
{
    auto ast = parseGraphOrThrow("MATCH (a) RETURN a UNION MATCH (b) RETURN b");
    const auto * set_query = ast->as<GAST::GQLSetQuery>();
    ASSERT_NE(set_query, nullptr);
    EXPECT_EQ(set_query->operation, GAST::SetOperation::Union);
    EXPECT_NE(set_query->left.get(), nullptr);
    EXPECT_NE(set_query->right.get(), nullptr);
}

TEST(GQLParser, NestedQuerySpecificationIsUnwrapped)
{
    auto ast = parseGraphOrThrow("{ MATCH (a) RETURN a }");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * match = getMatchClause(*clauses);
    const auto * project = getProjectClause(*clauses);
    ASSERT_NE(match, nullptr);
    ASSERT_NE(project, nullptr);
}

TEST(GQLParser, StandaloneOrderByStatementFallsBackToRawTextClause)
{
    auto ast = parseGraphOrThrow("MATCH (a) ORDER BY a DESC LIMIT 3 RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 3);

    const auto * clause = getExpr(clauses->clauses[1]);
    ASSERT_NE(clause, nullptr);
    EXPECT_EQ(clause->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(clause->text, "ORDER BY a DESC LIMIT 3");
}

TEST(GQLParser, FinishStatementFallsBackToRawTextClause)
{
    auto ast = parseGraphOrThrow("MATCH (a) FINISH");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * finish = getExpr(clauses->clauses[1]);
    ASSERT_NE(finish, nullptr);
    EXPECT_EQ(finish->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(finish->text, "FINISH");
}

TEST(GQLParser, ReturnClauseKeepsGroupBy)
{
    auto ast = parseGraphOrThrow("MATCH (a) RETURN a GROUP BY a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);

    const auto * project = getProjectClause(*clauses);
    ASSERT_NE(project, nullptr);

    const auto * group_by = getExpr(project->group_by);
    ASSERT_NE(group_by, nullptr);
    EXPECT_EQ(group_by->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(group_by->text, "GROUP BY a");
    EXPECT_EQ(formatAST(*project), "RETURN a GROUP BY a");
}

TEST(GQLParser, FormatNodePattern)
{
    auto node = make_intrusive<GAST::GQLNodePattern>();
    node->variable = GAST::GQLExpr::identifier("n");
    node->label_expression = GAST::GQLLabelExpression::name("Person");
    attachChildIfPresent(*node, node->variable);
    attachChildIfPresent(*node, node->label_expression);

    EXPECT_EQ(formatAST(*node), "(n:Person)");
}

TEST(GQLParser, FormatEdgePatternWithQuantifier)
{
    auto edge = make_intrusive<GAST::GQLEdgePattern>(GAST::EdgeDirection::Right);
    edge->variable = GAST::GQLExpr::identifier("e");
    edge->label_expression = GAST::GQLLabelExpression::name("KNOWS");
    edge->quantifier = GAST::Ptr(make_intrusive<GAST::GQLQuantifier>(GAST::GQLQuantifier::Kind::Plus));
    attachChildIfPresent(*edge, edge->variable);
    attachChildIfPresent(*edge, edge->label_expression);
    attachChildIfPresent(*edge, edge->quantifier);

    EXPECT_EQ(formatAST(*edge), "-[e:KNOWS]->+");
}

TEST(GQLParser, CloneEdgeWithQuantifier)
{
    auto edge = make_intrusive<GAST::GQLEdgePattern>(GAST::EdgeDirection::Right);
    edge->variable = GAST::GQLExpr::identifier("e");
    edge->label_expression = GAST::GQLLabelExpression::name("KNOWS");
    edge->quantifier = GAST::Ptr(make_intrusive<GAST::GQLQuantifier>(GAST::GQLQuantifier::Kind::Range, "1", "5"));
    attachChildIfPresent(*edge, edge->variable);
    attachChildIfPresent(*edge, edge->label_expression);
    attachChildIfPresent(*edge, edge->quantifier);

    auto cloned = edge->clone();
    const auto * cloned_edge = cloned->as<GAST::GQLEdgePattern>();
    ASSERT_NE(cloned_edge, nullptr);
    EXPECT_EQ(cloned_edge->direction, GAST::EdgeDirection::Right);
    EXPECT_NE(cloned_edge->variable.get(), edge->variable.get());
    EXPECT_NE(cloned_edge->quantifier.get(), edge->quantifier.get());
    EXPECT_EQ(formatAST(*cloned_edge), "-[e:KNOWS]->{1, 5}");
}

TEST(GQLParser, InvalidSyntaxThrowsException)
{
    try
    {
        (void)parseGraphOrThrow("MATCH INVALID SYNTAX !!!");
        FAIL() << "Expected `DB::Exception`";
    }
    catch (const DB::Exception & e)
    {
        EXPECT_FALSE(e.message().empty());
    }
}

#endif
