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

const GAST::GQLCallClause * getCallClause(const GAST::GQLClausesQuery & clauses, size_t index = 0)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * call = clauses.clauses[index]->as<GAST::GQLCallClause>();
    EXPECT_NE(call, nullptr);
    return call;
}

const GAST::GQLLetClause * getLetClause(const GAST::GQLClausesQuery & clauses, size_t index = 0)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * let_clause = clauses.clauses[index]->as<GAST::GQLLetClause>();
    EXPECT_NE(let_clause, nullptr);
    return let_clause;
}

const GAST::GQLForClause * getForClause(const GAST::GQLClausesQuery & clauses, size_t index = 0)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * for_clause = clauses.clauses[index]->as<GAST::GQLForClause>();
    EXPECT_NE(for_clause, nullptr);
    return for_clause;
}

const GAST::GQLFinishClause * getFinishClause(const GAST::GQLClausesQuery & clauses, size_t index)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * finish_clause = clauses.clauses[index]->as<GAST::GQLFinishClause>();
    EXPECT_NE(finish_clause, nullptr);
    return finish_clause;
}

const GAST::GQLAssignmentItem * getAssignmentItem(const ASTPtr & ast)
{
    const auto * assignment = ast->as<GAST::GQLAssignmentItem>();
    EXPECT_NE(assignment, nullptr);
    return assignment;
}

const GAST::GQLSelectSourceItem * getSelectSourceItem(const ASTPtr & ast)
{
    const auto * source_item = ast->as<GAST::GQLSelectSourceItem>();
    EXPECT_NE(source_item, nullptr);
    return source_item;
}

const GAST::GQLSelectSourceList * getSelectSourceList(const ASTPtr & ast)
{
    const auto * source_list = ast->as<GAST::GQLSelectSourceList>();
    EXPECT_NE(source_list, nullptr);
    return source_list;
}

const GAST::GQLPageClause * getPageClause(const GAST::GQLClausesQuery & clauses, size_t index)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * page_clause = clauses.clauses[index]->as<GAST::GQLPageClause>();
    EXPECT_NE(page_clause, nullptr);
    return page_clause;
}

const GAST::GQLUseClause * getUseClause(const GAST::GQLClausesQuery & clauses, size_t index = 0)
{
    if (index >= clauses.clauses.size())
        return nullptr;

    const auto * use_clause = clauses.clauses[index]->as<GAST::GQLUseClause>();
    EXPECT_NE(use_clause, nullptr);
    return use_clause;
}

const GAST::GQLSubqueryClause * getSubqueryClause(const ASTPtr & ast)
{
    const auto * subquery_clause = ast->as<GAST::GQLSubqueryClause>();
    EXPECT_NE(subquery_clause, nullptr);
    return subquery_clause;
}

const GAST::GQLSubqueryNextClause * getSubqueryNextClause(const ASTPtr & ast)
{
    const auto * next_clause = ast->as<GAST::GQLSubqueryNextClause>();
    EXPECT_NE(next_clause, nullptr);
    return next_clause;
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

    const auto * use_clause = getUseClause(*clauses, 0);
    ASSERT_NE(use_clause, nullptr);
    const auto * graph_reference = getExpr(use_clause->graph_reference);
    ASSERT_NE(graph_reference, nullptr);
    EXPECT_EQ(graph_reference->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(graph_reference->text, "foo");
    EXPECT_EQ(formatAST(*use_clause), "USE foo");

    const auto * match = getMatchClause(*clauses, 1);
    const auto * project = getProjectClause(*clauses, 2);
    ASSERT_NE(match, nullptr);
    ASSERT_NE(project, nullptr);
}

TEST(GQLParser, FocusedNestedQueryKeepsSubqueryWrapper)
{
    auto ast = parseGraphOrThrow("USE foo { MATCH (a) RETURN a }");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * use_clause = getUseClause(*clauses, 0);
    ASSERT_NE(use_clause, nullptr);
    const auto * graph_reference = getExpr(use_clause->graph_reference);
    ASSERT_NE(graph_reference, nullptr);
    EXPECT_EQ(graph_reference->text, "foo");
    EXPECT_EQ(formatAST(*use_clause), "USE foo");

    const auto * subquery_clause = getSubqueryClause(clauses->clauses[1]);
    ASSERT_NE(subquery_clause, nullptr);
    ASSERT_NE(getClausesQuery(subquery_clause->statement), nullptr);
    EXPECT_EQ(formatAST(*subquery_clause), "{ MATCH (a) RETURN a }");
}

TEST(GQLParser, SelectStatementBuildsTopLevelProject)
{
    auto ast = parseGraphOrThrow("SELECT DISTINCT a AS x FROM { MATCH (a) RETURN a } WHERE a IS NOT NULL GROUP BY a HAVING a IS NOT NULL ORDER BY a DESC OFFSET 1 LIMIT 2");
    const auto * project = ast->as<GAST::GQLProjectClause>();
    ASSERT_NE(project, nullptr);
    EXPECT_EQ(project->type, GAST::GQLProjectClause::Type::Select);
    EXPECT_TRUE(project->distinct);
    ASSERT_EQ(project->items.size(), 1);

    const auto * item = getExpr(project->items[0]);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->text, "a");
    EXPECT_EQ(item->tryGetAlias(), "x");

    const auto * source = getSubqueryClause(project->source);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(getClausesQuery(source->statement), nullptr);

    const auto * where = project->where->as<GAST::GQLWhereClause>();
    ASSERT_NE(where, nullptr);
    const auto * having = project->having->as<GAST::GQLWhereClause>();
    ASSERT_NE(having, nullptr);
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
    EXPECT_EQ(formatAST(*project), "SELECT DISTINCT a AS x FROM { MATCH (a) RETURN a } WHERE (a IS NOT NULL) GROUP BY a HAVING (a IS NOT NULL) ORDER BY a DESC OFFSET 1 LIMIT 2");
}

TEST(GQLParser, SelectStatementPreservesGraphQualifiedQuerySource)
{
    auto ast = parseGraphOrThrow("SELECT a FROM foo { MATCH (a) RETURN a }");
    const auto * project = ast->as<GAST::GQLProjectClause>();
    ASSERT_NE(project, nullptr);

    const auto * source_item = getSelectSourceItem(project->source);
    ASSERT_NE(source_item, nullptr);

    const auto * graph_reference = getExpr(source_item->graph_reference);
    ASSERT_NE(graph_reference, nullptr);
    EXPECT_EQ(graph_reference->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(graph_reference->text, "foo");

    const auto * nested_query = getSubqueryClause(source_item->source);
    ASSERT_NE(nested_query, nullptr);
    ASSERT_NE(getClausesQuery(nested_query->statement), nullptr);
    EXPECT_EQ(formatAST(*project), "SELECT a FROM foo { MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryKeepsNextStatementsInsideSubqueryClause)
{
    auto ast = parseGraphOrThrow("USE foo { MATCH (a) RETURN a NEXT YIELD a RETURN a }");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * subquery_clause = getSubqueryClause(clauses->clauses[1]);
    ASSERT_NE(subquery_clause, nullptr);
    ASSERT_EQ(subquery_clause->next_statements.size(), 1);

    const auto * next_clause = getSubqueryNextClause(subquery_clause->next_statements[0]);
    ASSERT_NE(next_clause, nullptr);
    const auto * yield_clause = getExpr(next_clause->yield);
    ASSERT_NE(yield_clause, nullptr);
    EXPECT_EQ(yield_clause->text, "YIELD a");
    ASSERT_NE(getClausesQuery(next_clause->statement), nullptr);
    EXPECT_EQ(formatAST(*subquery_clause), "{ MATCH (a) RETURN a NEXT YIELD a RETURN a }");
}

TEST(GQLParser, SelectStatementBuildsStructuredGraphMatchSourceList)
{
    auto ast = parseGraphOrThrow("SELECT a, b FROM foo MATCH (a), bar MATCH (b) WHERE a = b");
    const auto * project = ast->as<GAST::GQLProjectClause>();
    ASSERT_NE(project, nullptr);

    const auto * source_list = getSelectSourceList(project->source);
    ASSERT_NE(source_list, nullptr);
    ASSERT_EQ(source_list->items.size(), 2);

    const auto * first_item = getSelectSourceItem(source_list->items[0]);
    ASSERT_NE(first_item, nullptr);
    EXPECT_EQ(getExpr(first_item->graph_reference)->text, "foo");
    ASSERT_NE(first_item->source->as<GAST::GQLMatchClause>(), nullptr);

    const auto * second_item = getSelectSourceItem(source_list->items[1]);
    ASSERT_NE(second_item, nullptr);
    EXPECT_EQ(getExpr(second_item->graph_reference)->text, "bar");
    ASSERT_NE(second_item->source->as<GAST::GQLMatchClause>(), nullptr);

    EXPECT_EQ(formatAST(*project), "SELECT a, b FROM foo MATCH (a), bar MATCH (b) WHERE (a = b)");
}

TEST(GQLParser, CallClauseBuildsStructuredNode)
{
    auto ast = parseGraphOrThrow("CALL foo(a, b) YIELD x AS y RETURN y");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * call = getCallClause(*clauses, 0);
    ASSERT_NE(call, nullptr);
    EXPECT_FALSE(call->optional);
    EXPECT_FALSE(call->inline_procedure);

    const auto * procedure = getExpr(call->procedure);
    ASSERT_NE(procedure, nullptr);
    EXPECT_EQ(procedure->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(procedure->text, "foo");

    ASSERT_EQ(call->arguments.size(), 2);
    EXPECT_EQ(getExpr(call->arguments[0])->text, "a");
    EXPECT_EQ(getExpr(call->arguments[1])->text, "b");

    ASSERT_EQ(call->yield_items.size(), 1);
    const auto * yield_item = getExpr(call->yield_items[0]);
    ASSERT_NE(yield_item, nullptr);
    EXPECT_EQ(yield_item->kind, GAST::GQLExpr::Kind::Identifier);
    EXPECT_EQ(yield_item->text, "x");
    EXPECT_EQ(yield_item->tryGetAlias(), "y");
    EXPECT_EQ(formatAST(*call), "CALL foo(a, b) YIELD x AS y");
}

TEST(GQLParser, InlineOptionalCallKeepsInlineProcedureShape)
{
    auto ast = parseGraphOrThrow("OPTIONAL CALL { RETURN 1 } RETURN *");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * call = getCallClause(*clauses, 0);
    ASSERT_NE(call, nullptr);
    EXPECT_TRUE(call->optional);
    EXPECT_TRUE(call->inline_procedure);
    EXPECT_TRUE(call->arguments.empty());
    EXPECT_TRUE(call->yield_items.empty());

    const auto * procedure = getExpr(call->procedure);
    ASSERT_NE(procedure, nullptr);
    EXPECT_EQ(procedure->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(procedure->text, "{RETURN1}");
    EXPECT_EQ(formatAST(*call), "OPTIONAL CALL {RETURN1}");
}

TEST(GQLParser, LetClauseBuildsStructuredItems)
{
    auto ast = parseGraphOrThrow("LET x = 1, VALUE y INT = 2 RETURN x");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * let_clause = getLetClause(*clauses, 0);
    ASSERT_NE(let_clause, nullptr);
    ASSERT_EQ(let_clause->items.size(), 2);

    const auto * binding_assignment = getAssignmentItem(let_clause->items[0]);
    ASSERT_NE(binding_assignment, nullptr);
    EXPECT_EQ(binding_assignment->name, "x");
    EXPECT_FALSE(binding_assignment->value_keyword);
    EXPECT_TRUE(binding_assignment->raw_type.empty());
    EXPECT_EQ(getExpr(binding_assignment->value)->text, "1");

    const auto * value_assignment = getAssignmentItem(let_clause->items[1]);
    ASSERT_NE(value_assignment, nullptr);
    EXPECT_EQ(value_assignment->name, "y");
    EXPECT_TRUE(value_assignment->value_keyword);
    EXPECT_EQ(value_assignment->raw_type, "INT");
    EXPECT_EQ(getExpr(value_assignment->value)->text, "2");
    EXPECT_EQ(formatAST(*let_clause), "LET x = 1, VALUE y INT = 2");
}

TEST(GQLParser, ForClauseBuildsStructuredFields)
{
    auto ast = parseGraphOrThrow("FOR x IN [1, 2] WITH OFFSET i RETURN x");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * for_clause = getForClause(*clauses, 0);
    ASSERT_NE(for_clause, nullptr);
    EXPECT_EQ(for_clause->alias, "x");
    EXPECT_FALSE(for_clause->with_ordinality);
    EXPECT_TRUE(for_clause->with_offset);
    EXPECT_EQ(for_clause->ordinality_or_offset_alias, "i");

    const auto * source = getExpr(for_clause->source);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->kind, GAST::GQLExpr::Kind::RawText);
    EXPECT_EQ(source->text, "[1,2]");
    EXPECT_EQ(formatAST(*for_clause), "FOR x IN [1,2] WITH OFFSET i");
}

TEST(GQLParser, FinishClauseBuildsStructuredNode)
{
    auto ast = parseGraphOrThrow("MATCH (a) FINISH");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 2);

    const auto * match = getMatchClause(*clauses, 0);
    ASSERT_NE(match, nullptr);
    const auto * finish_clause = getFinishClause(*clauses, 1);
    ASSERT_NE(finish_clause, nullptr);
    EXPECT_EQ(formatAST(*finish_clause), "FINISH");
    EXPECT_EQ(formatAST(*clauses), "MATCH (a) FINISH");
}

TEST(GQLParser, StandalonePagingBuildsStructuredClause)
{
    auto ast = parseGraphOrThrow("MATCH (a) ORDER BY a DESC OFFSET 1 LIMIT 2 RETURN a");
    const auto * clauses = getClausesQuery(ast);
    ASSERT_NE(clauses, nullptr);
    ASSERT_EQ(clauses->clauses.size(), 3);

    ASSERT_NE(getMatchClause(*clauses, 0), nullptr);

    const auto * page_clause = getPageClause(*clauses, 1);
    ASSERT_NE(page_clause, nullptr);

    const auto * order_by = page_clause->order_by->as<GAST::GQLOrderByClause>();
    ASSERT_NE(order_by, nullptr);
    const auto * offset = getExpr(page_clause->offset);
    const auto * limit = getExpr(page_clause->limit);
    ASSERT_NE(offset, nullptr);
    ASSERT_NE(limit, nullptr);
    EXPECT_EQ(offset->text, "1");
    EXPECT_EQ(limit->text, "2");
    EXPECT_EQ(formatAST(*page_clause), "ORDER BY a DESC OFFSET 1 LIMIT 2");

    ASSERT_NE(getProjectClause(*clauses, 2), nullptr);
    EXPECT_EQ(formatAST(*clauses), "MATCH (a) ORDER BY a DESC OFFSET 1 LIMIT 2 RETURN a");
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
