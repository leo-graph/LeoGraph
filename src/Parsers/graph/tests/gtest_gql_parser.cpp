#include "config.h"

#if USE_GQL_GRAMMAR

#  include <Common/Exception.h>
#  include <gtest/gtest.h>
#  include <IO/WriteBufferFromString.h>
#  include <Parsers/graph/GraphAST.h>
#  include <Parsers/graph/ParserGraphQuery.h>
#  include <Parsers/parseQuery.h>
#  include <Parsers/ParserQuery.h>

using namespace DB;

namespace GAST = DB::OPENGQL::AST;

namespace {

ASTPtr parseGraphOrThrow(std::string_view query) { return DB::parseQuery(query); }

ASTPtr parseTopLevelOrThrow(const String& query) {
  ParserQuery parser(query.data() + query.size());
  return DB::parseQuery(parser, query, 0, 0, 0);
}

String formatAST(const IAST& ast) {
  WriteBufferFromOwnString out;
  IAST::FormatSettings settings(false);
  IAST::FormatState state;
  ast.format(out, settings, state, {});
  return out.str();
}

const GAST::GQLSingleQuery* getSingleQuery(const ASTPtr& ast) {
  const auto* query = ast->as<GAST::GQLSingleQuery>();
  EXPECT_NE(query, nullptr);
  return query;
}

const GAST::GQLSingleQuery* getClausesQuery(const ASTPtr& ast) { return getSingleQuery(ast); }

const GAST::GQLMatchClause* getMatchClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* match = clauses.clauses[index]->as<GAST::GQLMatchClause>();
  EXPECT_NE(match, nullptr);
  return match;
}

const GAST::GQLSelectClause* getSelectClause(const GAST::GQLSingleQuery& query, size_t index = 0) {
  if (index >= query.clauses.size()) return nullptr;

  const auto* clause = query.clauses[index]->as<GAST::GQLSelectClause>();
  EXPECT_NE(clause, nullptr);
  return clause;
}

const GAST::GQLReturnClause* getReturnClause(const GAST::GQLSingleQuery& clauses, size_t index = 1) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* clause = clauses.clauses[index]->as<GAST::GQLReturnClause>();
  EXPECT_NE(clause, nullptr);
  return clause;
}

const GAST::GQLNamedCallClause* getNamedCallClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* call = clauses.clauses[index]->as<GAST::GQLNamedCallClause>();
  EXPECT_NE(call, nullptr);
  return call;
}

const GAST::GQLInlineCallClause* getInlineCallClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* call = clauses.clauses[index]->as<GAST::GQLInlineCallClause>();
  EXPECT_NE(call, nullptr);
  return call;
}

const GAST::GQLLetClause* getLetClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* let_clause = clauses.clauses[index]->as<GAST::GQLLetClause>();
  EXPECT_NE(let_clause, nullptr);
  return let_clause;
}

const GAST::GQLForClause* getForClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* for_clause = clauses.clauses[index]->as<GAST::GQLForClause>();
  EXPECT_NE(for_clause, nullptr);
  return for_clause;
}

const GAST::GQLGroupByClause* getGroupByClause(const ASTPtr& ast) {
  const auto* group_by_clause = ast->as<GAST::GQLGroupByClause>();
  EXPECT_NE(group_by_clause, nullptr);
  return group_by_clause;
}

const GAST::GQLFinishClause* getFinishClause(const GAST::GQLSingleQuery& clauses, size_t index) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* finish_clause = clauses.clauses[index]->as<GAST::GQLFinishClause>();
  EXPECT_NE(finish_clause, nullptr);
  return finish_clause;
}

const GAST::GQLAssignmentItem* getAssignmentItem(const ASTPtr& ast) {
  const auto* assignment = ast->as<GAST::GQLAssignmentItem>();
  EXPECT_NE(assignment, nullptr);
  return assignment;
}

const GAST::GQLSelectSourceItem* getSelectSourceItem(const ASTPtr& ast) {
  const auto* source_item = ast->as<GAST::GQLSelectSourceItem>();
  EXPECT_NE(source_item, nullptr);
  return source_item;
}

const GAST::GQLSelectSourceList* getSelectSourceList(const ASTPtr& ast) {
  const auto* source_list = ast->as<GAST::GQLSelectSourceList>();
  EXPECT_NE(source_list, nullptr);
  return source_list;
}

const GAST::GQLPageClause* getPageClause(const GAST::GQLSingleQuery& clauses, size_t index) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* page_clause = clauses.clauses[index]->as<GAST::GQLPageClause>();
  EXPECT_NE(page_clause, nullptr);
  return page_clause;
}

const GAST::GQLUseClause* getUseClause(const GAST::GQLSingleQuery& clauses, size_t index = 0) {
  if (index >= clauses.clauses.size()) return nullptr;

  const auto* use_clause = clauses.clauses[index]->as<GAST::GQLUseClause>();
  EXPECT_NE(use_clause, nullptr);
  return use_clause;
}

const GAST::GQLSubquery* getSubquery(const ASTPtr& ast) {
  const auto* subquery = ast->as<GAST::GQLSubquery>();
  EXPECT_NE(subquery, nullptr);
  return subquery;
}

const GAST::GQLAtSchemaClause* getAtSchemaClause(const ASTPtr& ast) {
  const auto* at_schema_clause = ast->as<GAST::GQLAtSchemaClause>();
  EXPECT_NE(at_schema_clause, nullptr);
  return at_schema_clause;
}

const GAST::GQLBindingVariableDefinitionBlock* getBindingVariableDefinitionBlock(const ASTPtr& ast) {
  const auto* block = ast->as<GAST::GQLBindingVariableDefinitionBlock>();
  EXPECT_NE(block, nullptr);
  return block;
}

const GAST::GQLBindingVariableDefinition* getBindingVariableDefinition(const ASTPtr& ast) {
  const auto* definition = ast->as<GAST::GQLBindingVariableDefinition>();
  EXPECT_NE(definition, nullptr);
  return definition;
}

const GAST::GQLBindingInitializer* getBindingInitializer(const ASTPtr& ast) {
  const auto* initializer = ast->as<GAST::GQLBindingInitializer>();
  EXPECT_NE(initializer, nullptr);
  return initializer;
}

const GAST::GQLGraphExpression* getGraphExpression(const ASTPtr& ast) {
  const auto* expression = ast->as<GAST::GQLGraphExpression>();
  EXPECT_NE(expression, nullptr);
  return expression;
}

const GAST::GQLBindingTableExpression* getBindingTableExpression(const ASTPtr& ast) {
  const auto* expression = ast->as<GAST::GQLBindingTableExpression>();
  EXPECT_NE(expression, nullptr);
  return expression;
}

const GAST::GQLSubqueryNextClause* getSubqueryNextClause(const ASTPtr& ast) {
  const auto* next_clause = ast->as<GAST::GQLSubqueryNextClause>();
  EXPECT_NE(next_clause, nullptr);
  return next_clause;
}

const GAST::GQLYieldClause* getYieldClause(const ASTPtr& ast) {
  const auto* yield_clause = ast->as<GAST::GQLYieldClause>();
  EXPECT_NE(yield_clause, nullptr);
  return yield_clause;
}

const GAST::GQLSchemaReference* getSchemaReference(const ASTPtr& ast) {
  const auto* schema_reference = ast->as<GAST::GQLSchemaReference>();
  EXPECT_NE(schema_reference, nullptr);
  return schema_reference;
}

const GAST::GQLPathPattern* getOnlyPathPattern(const GAST::GQLMatchClause& match) {
  if (match.path_patterns.size() != 1) return nullptr;

  const auto* path = match.path_patterns.front()->as<GAST::GQLPathPattern>();
  EXPECT_NE(path, nullptr);
  return path;
}

const GAST::GQLPathPatternPrefix* getPathPatternPrefix(const ASTPtr& ast) {
  const auto* prefix = ast->as<GAST::GQLPathPatternPrefix>();
  EXPECT_NE(prefix, nullptr);
  return prefix;
}

const GAST::GQLParenthesizedPathPattern* getParenthesizedPathPattern(const ASTPtr& ast) {
  const auto* path = ast->as<GAST::GQLParenthesizedPathPattern>();
  EXPECT_NE(path, nullptr);
  return path;
}

const GAST::GQLMatchStatementBlock* getMatchStatementBlock(const ASTPtr& ast) {
  const auto* block = ast->as<GAST::GQLMatchStatementBlock>();
  EXPECT_NE(block, nullptr);
  return block;
}

const GAST::GQLGraphPatternBlock* getGraphPatternBlock(const ASTPtr& ast) {
  const auto* block = ast->as<GAST::GQLGraphPatternBlock>();
  EXPECT_NE(block, nullptr);
  return block;
}

const GAST::GQLListConstructor* getListConstructor(const ASTPtr& ast) {
  const auto* constructor = ast->as<GAST::GQLListConstructor>();
  EXPECT_NE(constructor, nullptr);
  return constructor;
}

const GAST::GQLRecordConstructor* getRecordConstructor(const ASTPtr& ast) {
  const auto* constructor = ast->as<GAST::GQLRecordConstructor>();
  EXPECT_NE(constructor, nullptr);
  return constructor;
}

const GAST::GQLExpr* getExpr(const ASTPtr& ast) {
  const auto* expression = ast->as<GAST::GQLExpr>();
  EXPECT_NE(expression, nullptr);
  return expression;
}

const GAST::GQLLabelExpression* getLabelExpr(const ASTPtr& ast) {
  const auto* expression = ast->as<GAST::GQLLabelExpression>();
  EXPECT_NE(expression, nullptr);
  return expression;
}

const GAST::GQLQuantifier* getQuantifier(const ASTPtr& ast) {
  const auto* quantifier = ast->as<GAST::GQLQuantifier>();
  EXPECT_NE(quantifier, nullptr);
  return quantifier;
}

void attachChildIfPresent(IAST& owner, const ASTPtr& child) {
  if (child) owner.children.push_back(child);
}

}  // namespace

TEST(GQLParser, SimpleMatchClause) {
  auto ast = parseGraphOrThrow("MATCH (n:Person) RETURN n");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  EXPECT_FALSE(match->optional);

  const auto* path = getOnlyPathPattern(*match);
  ASSERT_NE(path, nullptr);
  ASSERT_EQ(path->elements.size(), 1);

  const auto* node = path->elements[0]->as<GAST::GQLNodePattern>();
  ASSERT_NE(node, nullptr);

  const auto* variable = getExpr(node->variable);
  ASSERT_NE(variable, nullptr);
  EXPECT_EQ(variable->kind, GAST::GQLExpr::Kind::Identifier);
  EXPECT_EQ(variable->text, "n");

  const auto* label = getLabelExpr(node->label_expression);
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->kind, GAST::GQLLabelExpression::Kind::Name);
  EXPECT_EQ(label->text, "Person");
}

TEST(GQLParser, OptionalMatchIsAcceptedByTopLevelParser) {
  auto ast = parseTopLevelOrThrow("OPTIONAL MATCH (a) RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* match = getMatchClause(*clauses, 0);
  ASSERT_NE(match, nullptr);
  EXPECT_TRUE(match->optional);

  const auto* return_clause = getReturnClause(*clauses, 1);
  ASSERT_NE(return_clause, nullptr);
  ASSERT_EQ(return_clause->items.size(), 1);
}

TEST(GQLParser, FocusedUseGraphQueryPreservesUseClause) {
  auto ast = parseGraphOrThrow("USE foo MATCH (a) RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 3);

  const auto* use_clause = getUseClause(*clauses, 0);
  ASSERT_NE(use_clause, nullptr);
  const auto* graph_reference = getGraphExpression(use_clause->graph_reference);
  ASSERT_NE(graph_reference, nullptr);
  EXPECT_EQ(graph_reference->kind, GAST::GQLGraphExpression::Kind::ObjectNameOrBindingVariable);
  EXPECT_EQ(graph_reference->text, "foo");
  EXPECT_EQ(formatAST(*use_clause), "USE foo");

  const auto* match = getMatchClause(*clauses, 1);
  const auto* project = getReturnClause(*clauses, 2);
  ASSERT_NE(match, nullptr);
  ASSERT_NE(project, nullptr);
}

TEST(GQLParser, FocusedNestedQueryKeepsSubqueryWrapper) {
  auto ast = parseGraphOrThrow("USE foo { MATCH (a) RETURN a }");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* use_clause = getUseClause(*clauses, 0);
  ASSERT_NE(use_clause, nullptr);
  const auto* graph_reference = getGraphExpression(use_clause->graph_reference);
  ASSERT_NE(graph_reference, nullptr);
  EXPECT_EQ(graph_reference->text, "foo");
  EXPECT_EQ(formatAST(*use_clause), "USE foo");

  const auto* subquery = getSubquery(clauses->clauses[1]);
  ASSERT_NE(subquery, nullptr);
  ASSERT_NE(getClausesQuery(subquery->query), nullptr);
  EXPECT_EQ(formatAST(*subquery), "{ MATCH (a) RETURN a }");
}

TEST(GQLParser, SelectStatementBuildsStructuredSingleQuery) {
  auto ast = parseGraphOrThrow(
      "SELECT DISTINCT a AS x FROM { MATCH (a) RETURN a } WHERE a IS NOT NULL GROUP BY a HAVING a IS NOT NULL ORDER BY a DESC OFFSET 1 "
      "LIMIT 2");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* select_clause = getSelectClause(*clauses, 0);
  ASSERT_NE(select_clause, nullptr);
  EXPECT_TRUE(select_clause->distinct);
  ASSERT_EQ(select_clause->items.size(), 1);

  const auto* item = getExpr(select_clause->items[0]);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->text, "a");
  EXPECT_EQ(item->tryGetAlias(), "x");

  const auto* source = getSubquery(select_clause->source);
  ASSERT_NE(source, nullptr);
  ASSERT_NE(getClausesQuery(source->query), nullptr);

  const auto* where = select_clause->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);
  const auto* having = select_clause->having->as<GAST::GQLWhereClause>();
  ASSERT_NE(having, nullptr);
  const auto* group_by = getGroupByClause(select_clause->group_by);
  ASSERT_NE(group_by, nullptr);
  ASSERT_EQ(group_by->items.size(), 1);
  EXPECT_EQ(getExpr(group_by->items[0])->text, "a");

  const auto* page_clause = getPageClause(*clauses, 1);
  ASSERT_NE(page_clause, nullptr);
  const auto* order_by = page_clause->order_by->as<GAST::GQLOrderByClause>();
  ASSERT_NE(order_by, nullptr);
  const auto* offset = getExpr(page_clause->offset);
  const auto* limit = getExpr(page_clause->limit);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(limit, nullptr);
  EXPECT_EQ(offset->text, "1");
  EXPECT_EQ(limit->text, "2");
  EXPECT_EQ(formatAST(*clauses),
            "SELECT DISTINCT a AS x FROM { MATCH (a) RETURN a } WHERE (a IS NOT NULL) GROUP BY a HAVING (a IS NOT NULL) ORDER BY a DESC "
            "OFFSET 1 LIMIT 2");
}

TEST(GQLParser, SelectStatementPreservesGraphQualifiedQuerySource) {
  auto ast = parseGraphOrThrow("SELECT a FROM foo { MATCH (a) RETURN a }");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 1);

  const auto* select_clause = getSelectClause(*clauses, 0);
  ASSERT_NE(select_clause, nullptr);

  const auto* source_item = getSelectSourceItem(select_clause->source);
  ASSERT_NE(source_item, nullptr);

  const auto* graph_reference = getGraphExpression(source_item->graph_reference);
  ASSERT_NE(graph_reference, nullptr);
  EXPECT_EQ(graph_reference->kind, GAST::GQLGraphExpression::Kind::ObjectNameOrBindingVariable);
  EXPECT_EQ(graph_reference->text, "foo");

  const auto* nested_query = getSubquery(source_item->source);
  ASSERT_NE(nested_query, nullptr);
  ASSERT_NE(getClausesQuery(nested_query->query), nullptr);
  EXPECT_EQ(formatAST(*clauses), "SELECT a FROM foo { MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryKeepsNextStatementsInsideSubqueryClause) {
  auto ast = parseGraphOrThrow("USE foo { MATCH (a) RETURN a NEXT YIELD a RETURN a }");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* subquery = getSubquery(clauses->clauses[1]);
  ASSERT_NE(subquery, nullptr);
  ASSERT_EQ(subquery->next_statements.size(), 1);

  const auto* next_clause = getSubqueryNextClause(subquery->next_statements[0]);
  ASSERT_NE(next_clause, nullptr);
  const auto* yield_clause = getYieldClause(next_clause->yield);
  ASSERT_NE(yield_clause, nullptr);
  ASSERT_EQ(yield_clause->items.size(), 1);
  EXPECT_EQ(getExpr(yield_clause->items[0])->text, "a");
  ASSERT_NE(getClausesQuery(next_clause->query), nullptr);
  EXPECT_EQ(formatAST(*subquery), "{ MATCH (a) RETURN a NEXT YIELD a RETURN a }");
}

TEST(GQLParser, NestedQueryPreservesStructuredSchemaAndBindings) {
  auto ast = parseGraphOrThrow("{ AT ../foo/bar/analytics VALUE x = 1 MATCH (a) RETURN a }");
  const auto* subquery = getSubquery(ast);
  ASSERT_NE(subquery, nullptr);

  const auto* at_schema_clause = getAtSchemaClause(subquery->at_schema);
  ASSERT_NE(at_schema_clause, nullptr);
  const auto* schema_reference = getSchemaReference(at_schema_clause->schema_reference);
  ASSERT_NE(schema_reference, nullptr);
  EXPECT_EQ(schema_reference->kind, GAST::GQLSchemaReference::Kind::RelativePath);
  EXPECT_EQ(schema_reference->parent_levels, 1);
  ASSERT_EQ(schema_reference->directory_parts.size(), 2);
  EXPECT_EQ(schema_reference->directory_parts[0], "foo");
  EXPECT_EQ(schema_reference->directory_parts[1], "bar");
  EXPECT_EQ(schema_reference->name, "analytics");

  const auto* binding_block = getBindingVariableDefinitionBlock(subquery->bindings);
  ASSERT_NE(binding_block, nullptr);
  ASSERT_EQ(binding_block->definitions.size(), 1);

  const auto* binding_definition = getBindingVariableDefinition(binding_block->definitions[0]);
  ASSERT_NE(binding_definition, nullptr);
  EXPECT_EQ(binding_definition->kind, GAST::GQLBindingVariableDefinition::Kind::Value);
  EXPECT_EQ(binding_definition->name, "x");
  EXPECT_TRUE(binding_definition->raw_type.empty());
  const auto* initializer = getBindingInitializer(binding_definition->initializer);
  ASSERT_NE(initializer, nullptr);
  EXPECT_EQ(initializer->kind, GAST::GQLBindingInitializer::Kind::Value);
  EXPECT_EQ(getExpr(initializer->value)->text, "1");

  ASSERT_NE(getClausesQuery(subquery->query), nullptr);
  EXPECT_EQ(formatAST(*subquery), "{ AT ../foo/bar/analytics VALUE x = 1 MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryPreservesStructuredBindingTableInitializer) {
  auto ast = parseGraphOrThrow("{ TABLE t = { MATCH (a) RETURN a } MATCH (a) RETURN a }");
  const auto* subquery = getSubquery(ast);
  ASSERT_NE(subquery, nullptr);

  const auto* binding_block = getBindingVariableDefinitionBlock(subquery->bindings);
  ASSERT_NE(binding_block, nullptr);
  ASSERT_EQ(binding_block->definitions.size(), 1);

  const auto* binding_definition = getBindingVariableDefinition(binding_block->definitions[0]);
  ASSERT_NE(binding_definition, nullptr);
  EXPECT_EQ(binding_definition->kind, GAST::GQLBindingVariableDefinition::Kind::BindingTable);
  EXPECT_EQ(binding_definition->name, "t");

  const auto* initializer = getBindingInitializer(binding_definition->initializer);
  ASSERT_NE(initializer, nullptr);
  EXPECT_EQ(initializer->kind, GAST::GQLBindingInitializer::Kind::BindingTable);

  const auto* binding_table_expression = getBindingTableExpression(initializer->value);
  ASSERT_NE(binding_table_expression, nullptr);
  EXPECT_EQ(binding_table_expression->kind, GAST::GQLBindingTableExpression::Kind::NestedQuery);

  const auto* nested_binding_query = getSubquery(binding_table_expression->value);
  ASSERT_NE(nested_binding_query, nullptr);
  ASSERT_NE(getClausesQuery(nested_binding_query->query), nullptr);
  EXPECT_EQ(formatAST(*subquery), "{ TABLE t = { MATCH (a) RETURN a } MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryPreservesStructuredGraphInitializer) {
  auto ast = parseGraphOrThrow("{ GRAPH g = CURRENT_GRAPH MATCH (a) RETURN a }");
  const auto* subquery = getSubquery(ast);
  ASSERT_NE(subquery, nullptr);

  const auto* binding_block = getBindingVariableDefinitionBlock(subquery->bindings);
  ASSERT_NE(binding_block, nullptr);
  ASSERT_EQ(binding_block->definitions.size(), 1);

  const auto* binding_definition = getBindingVariableDefinition(binding_block->definitions[0]);
  ASSERT_NE(binding_definition, nullptr);
  EXPECT_EQ(binding_definition->kind, GAST::GQLBindingVariableDefinition::Kind::Graph);
  EXPECT_EQ(binding_definition->name, "g");

  const auto* initializer = getBindingInitializer(binding_definition->initializer);
  ASSERT_NE(initializer, nullptr);
  EXPECT_EQ(initializer->kind, GAST::GQLBindingInitializer::Kind::Graph);

  const auto* graph_expression = getGraphExpression(initializer->value);
  ASSERT_NE(graph_expression, nullptr);
  EXPECT_EQ(graph_expression->kind, GAST::GQLGraphExpression::Kind::CurrentGraph);
  EXPECT_EQ(graph_expression->text, "CURRENT_GRAPH");
  EXPECT_EQ(formatAST(*subquery), "{ GRAPH g = CURRENT_GRAPH MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryPreservesStructuredBindingTableReferenceInitializer) {
  auto ast = parseGraphOrThrow("{ TABLE t = foo MATCH (a) RETURN a }");
  const auto* subquery = getSubquery(ast);
  ASSERT_NE(subquery, nullptr);

  const auto* binding_block = getBindingVariableDefinitionBlock(subquery->bindings);
  ASSERT_NE(binding_block, nullptr);
  ASSERT_EQ(binding_block->definitions.size(), 1);

  const auto* binding_definition = getBindingVariableDefinition(binding_block->definitions[0]);
  ASSERT_NE(binding_definition, nullptr);
  EXPECT_EQ(binding_definition->kind, GAST::GQLBindingVariableDefinition::Kind::BindingTable);
  EXPECT_EQ(binding_definition->name, "t");

  const auto* initializer = getBindingInitializer(binding_definition->initializer);
  ASSERT_NE(initializer, nullptr);
  EXPECT_EQ(initializer->kind, GAST::GQLBindingInitializer::Kind::BindingTable);

  const auto* binding_table_expression = getBindingTableExpression(initializer->value);
  ASSERT_NE(binding_table_expression, nullptr);
  EXPECT_EQ(binding_table_expression->kind, GAST::GQLBindingTableExpression::Kind::ObjectNameOrBindingVariable);
  EXPECT_EQ(binding_table_expression->text, "foo");
  EXPECT_EQ(formatAST(*subquery), "{ TABLE t = foo MATCH (a) RETURN a }");
}

TEST(GQLParser, NestedQueryRejectsNonQueryStatement) {
  try {
    (void)parseGraphOrThrow("{ INSERT () }");
    FAIL() << "Expected `DB::Exception`";
  } catch (const DB::Exception& e) {
    EXPECT_NE(e.message().find("nested non-query statement"), String::npos);
  }
}

TEST(GQLParser, SelectStatementBuildsStructuredGraphMatchSourceList) {
  auto ast = parseGraphOrThrow("SELECT a, b FROM foo MATCH (a), bar MATCH (b) WHERE a = b");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 1);

  const auto* select_clause = getSelectClause(*clauses, 0);
  ASSERT_NE(select_clause, nullptr);

  const auto* source_list = getSelectSourceList(select_clause->source);
  ASSERT_NE(source_list, nullptr);
  ASSERT_EQ(source_list->items.size(), 2);

  const auto* first_item = getSelectSourceItem(source_list->items[0]);
  ASSERT_NE(first_item, nullptr);
  const auto* first_graph = getGraphExpression(first_item->graph_reference);
  ASSERT_NE(first_graph, nullptr);
  EXPECT_EQ(first_graph->kind, GAST::GQLGraphExpression::Kind::ObjectNameOrBindingVariable);
  EXPECT_EQ(first_graph->text, "foo");
  ASSERT_NE(first_item->source->as<GAST::GQLMatchClause>(), nullptr);

  const auto* second_item = getSelectSourceItem(source_list->items[1]);
  ASSERT_NE(second_item, nullptr);
  const auto* second_graph = getGraphExpression(second_item->graph_reference);
  ASSERT_NE(second_graph, nullptr);
  EXPECT_EQ(second_graph->kind, GAST::GQLGraphExpression::Kind::ObjectNameOrBindingVariable);
  EXPECT_EQ(second_graph->text, "bar");
  ASSERT_NE(second_item->source->as<GAST::GQLMatchClause>(), nullptr);

  EXPECT_EQ(formatAST(*clauses), "SELECT a, b FROM foo MATCH (a), bar MATCH (b) WHERE (a = b)");
}

TEST(GQLParser, NamedCallClauseBuildsStructuredNode) {
  auto ast = parseGraphOrThrow("CALL foo(a, b) YIELD x AS y RETURN y");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* call = getNamedCallClause(*clauses, 0);
  ASSERT_NE(call, nullptr);
  EXPECT_FALSE(call->optional);

  const auto* procedure = getExpr(call->procedure);
  ASSERT_NE(procedure, nullptr);
  EXPECT_EQ(procedure->kind, GAST::GQLExpr::Kind::Identifier);
  EXPECT_EQ(procedure->text, "foo");

  ASSERT_EQ(call->arguments.size(), 2);
  EXPECT_EQ(getExpr(call->arguments[0])->text, "a");
  EXPECT_EQ(getExpr(call->arguments[1])->text, "b");

  const auto* yield_clause = getYieldClause(call->yield);
  ASSERT_NE(yield_clause, nullptr);
  ASSERT_EQ(yield_clause->items.size(), 1);
  const auto* yield_item = getExpr(yield_clause->items[0]);
  ASSERT_NE(yield_item, nullptr);
  EXPECT_EQ(yield_item->kind, GAST::GQLExpr::Kind::Identifier);
  EXPECT_EQ(yield_item->text, "x");
  EXPECT_EQ(yield_item->tryGetAlias(), "y");
  EXPECT_EQ(formatAST(*call), "CALL foo(a, b) YIELD x AS y");
}

TEST(GQLParser, InlineOptionalCallKeepsInlineProcedureShape) {
  auto ast = parseGraphOrThrow("OPTIONAL CALL { RETURN 1 } RETURN *");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* call = getInlineCallClause(*clauses, 0);
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->optional);

  const auto* procedure = getSubquery(call->subquery);
  ASSERT_NE(procedure, nullptr);
  ASSERT_NE(getClausesQuery(procedure->query), nullptr);
  EXPECT_EQ(formatAST(*call), "OPTIONAL CALL { RETURN 1 }");
}

TEST(GQLParser, LetClauseBuildsStructuredItems) {
  auto ast = parseGraphOrThrow("LET x = 1, VALUE y INT = 2 RETURN x");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* let_clause = getLetClause(*clauses, 0);
  ASSERT_NE(let_clause, nullptr);
  ASSERT_EQ(let_clause->items.size(), 2);

  const auto* binding_assignment = getAssignmentItem(let_clause->items[0]);
  ASSERT_NE(binding_assignment, nullptr);
  EXPECT_EQ(binding_assignment->name, "x");
  EXPECT_FALSE(binding_assignment->value_keyword);
  EXPECT_TRUE(binding_assignment->raw_type.empty());
  EXPECT_EQ(getExpr(binding_assignment->value)->text, "1");

  const auto* value_assignment = getAssignmentItem(let_clause->items[1]);
  ASSERT_NE(value_assignment, nullptr);
  EXPECT_EQ(value_assignment->name, "y");
  EXPECT_TRUE(value_assignment->value_keyword);
  EXPECT_EQ(value_assignment->raw_type, "INT");
  EXPECT_EQ(getExpr(value_assignment->value)->text, "2");
  EXPECT_EQ(formatAST(*let_clause), "LET x = 1, VALUE y INT = 2");
}

TEST(GQLParser, ForClauseBuildsStructuredFields) {
  auto ast = parseGraphOrThrow("FOR x IN [1, 2] WITH OFFSET i RETURN x");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* for_clause = getForClause(*clauses, 0);
  ASSERT_NE(for_clause, nullptr);
  EXPECT_EQ(for_clause->alias, "x");
  EXPECT_FALSE(for_clause->with_ordinality);
  EXPECT_TRUE(for_clause->with_offset);
  EXPECT_EQ(for_clause->ordinality_or_offset_alias, "i");

  const auto* source = getListConstructor(for_clause->source);
  ASSERT_NE(source, nullptr);
  ASSERT_EQ(source->items.size(), 2);
  EXPECT_EQ(getExpr(source->items[0])->text, "1");
  EXPECT_EQ(getExpr(source->items[1])->text, "2");
  EXPECT_EQ(formatAST(*for_clause), "FOR x IN [1, 2] WITH OFFSET i");
}

TEST(GQLParser, FinishClauseBuildsStructuredNode) {
  auto ast = parseGraphOrThrow("MATCH (a) FINISH");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* match = getMatchClause(*clauses, 0);
  ASSERT_NE(match, nullptr);
  const auto* finish_clause = getFinishClause(*clauses, 1);
  ASSERT_NE(finish_clause, nullptr);
  EXPECT_EQ(formatAST(*finish_clause), "FINISH");
  EXPECT_EQ(formatAST(*clauses), "MATCH (a) FINISH");
}

TEST(GQLParser, StandalonePagingBuildsStructuredClause) {
  auto ast = parseGraphOrThrow("MATCH (a) ORDER BY a DESC OFFSET 1 LIMIT 2 RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 3);

  ASSERT_NE(getMatchClause(*clauses, 0), nullptr);

  const auto* page_clause = getPageClause(*clauses, 1);
  ASSERT_NE(page_clause, nullptr);

  const auto* order_by = page_clause->order_by->as<GAST::GQLOrderByClause>();
  ASSERT_NE(order_by, nullptr);
  const auto* offset = getExpr(page_clause->offset);
  const auto* limit = getExpr(page_clause->limit);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(limit, nullptr);
  EXPECT_EQ(offset->text, "1");
  EXPECT_EQ(limit->text, "2");
  EXPECT_EQ(formatAST(*page_clause), "ORDER BY a DESC OFFSET 1 LIMIT 2");

  ASSERT_NE(getReturnClause(*clauses, 2), nullptr);
  EXPECT_EQ(formatAST(*clauses), "MATCH (a) ORDER BY a DESC OFFSET 1 LIMIT 2 RETURN a");
}

TEST(GQLParser, RightEdgeWithRangeQuantifier) {
  auto ast = parseGraphOrThrow("MATCH (a)-[e:KNOWS]->{2,5}(b) RETURN e");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* path = getOnlyPathPattern(*match);
  ASSERT_NE(path, nullptr);
  ASSERT_EQ(path->elements.size(), 3);

  const auto* edge = path->elements[1]->as<GAST::GQLEdgePattern>();
  ASSERT_NE(edge, nullptr);
  EXPECT_EQ(edge->direction, GAST::EdgeDirection::Right);

  const auto* variable = getExpr(edge->variable);
  ASSERT_NE(variable, nullptr);
  EXPECT_EQ(variable->text, "e");

  const auto* label = getLabelExpr(edge->label_expression);
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->text, "KNOWS");

  const auto* quantifier = getQuantifier(edge->quantifier);
  ASSERT_NE(quantifier, nullptr);
  EXPECT_EQ(quantifier->kind, GAST::GQLQuantifier::Kind::Range);
  EXPECT_EQ(quantifier->lower, "2");
  EXPECT_EQ(quantifier->upper, "5");
}

TEST(GQLParser, LabelConjunction) {
  auto ast = parseGraphOrThrow("MATCH (n:Person&Employee) RETURN n");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* path = getOnlyPathPattern(*match);
  ASSERT_NE(path, nullptr);

  const auto* node = path->elements[0]->as<GAST::GQLNodePattern>();
  ASSERT_NE(node, nullptr);

  const auto* conjunction = getLabelExpr(node->label_expression);
  ASSERT_NE(conjunction, nullptr);
  EXPECT_EQ(conjunction->kind, GAST::GQLLabelExpression::Kind::Conjunction);
  ASSERT_EQ(conjunction->children.size(), 2);

  const auto* left = getLabelExpr(conjunction->children[0]);
  const auto* right = getLabelExpr(conjunction->children[1]);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->text, "Person");
  EXPECT_EQ(right->text, "Employee");
}

TEST(GQLParser, WhereExpressionKeepsStructure) {
  auto ast = parseGraphOrThrow("MATCH (a:Person) WHERE a.age > 20 AND a.age < 40 RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* root = getExpr(where->expression);
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->kind, GAST::GQLExpr::Kind::BinaryOp);
  EXPECT_EQ(root->text, "AND");
  ASSERT_EQ(root->children.size(), 2);

  const auto* left = getExpr(root->children[0]);
  const auto* right = getExpr(root->children[1]);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->text, ">");
  EXPECT_EQ(right->text, "<");
}

TEST(GQLParser, PropertyAccessExpression) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE a.name = 'Bob' RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* equals = getExpr(where->expression);
  ASSERT_NE(equals, nullptr);
  EXPECT_EQ(equals->text, "=");
  ASSERT_EQ(equals->children.size(), 2);

  const auto* property = getExpr(equals->children[0]);
  ASSERT_NE(property, nullptr);
  EXPECT_EQ(property->kind, GAST::GQLExpr::Kind::Property);
  EXPECT_EQ(property->text, "name");
  ASSERT_EQ(property->children.size(), 1);

  const auto* identifier = getExpr(property->children[0]);
  ASSERT_NE(identifier, nullptr);
  EXPECT_EQ(identifier->kind, GAST::GQLExpr::Kind::Identifier);
  EXPECT_EQ(identifier->text, "a");

  const auto* literal = getExpr(equals->children[1]);
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->kind, GAST::GQLExpr::Kind::Literal);
  EXPECT_EQ(literal->text, "'Bob'");
}

TEST(GQLParser, TruthValuePredicateKeepsStructure) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE a IS NOT TRUE RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* predicate = getExpr(where->expression);
  ASSERT_NE(predicate, nullptr);
  EXPECT_EQ(predicate->kind, GAST::GQLExpr::Kind::BinaryOp);
  EXPECT_EQ(predicate->text, "IS NOT");

  const auto* left = getExpr(predicate->children[0]);
  const auto* right = getExpr(predicate->children[1]);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->text, "a");
  EXPECT_EQ(right->kind, GAST::GQLExpr::Kind::Literal);
  EXPECT_EQ(right->text, "TRUE");
}

TEST(GQLParser, NullPredicateKeepsStructure) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE a.name IS NOT NULL RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* predicate = getExpr(where->expression);
  ASSERT_NE(predicate, nullptr);
  EXPECT_EQ(predicate->text, "IS NOT");

  const auto* property = getExpr(predicate->children[0]);
  const auto* null_literal = getExpr(predicate->children[1]);
  ASSERT_NE(property, nullptr);
  ASSERT_NE(null_literal, nullptr);
  EXPECT_EQ(property->kind, GAST::GQLExpr::Kind::Property);
  EXPECT_EQ(property->text, "name");
  EXPECT_EQ(null_literal->text, "NULL");
}

TEST(GQLParser, PropertyExistsPredicateBecomesFunctionCall) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE PROPERTY_EXISTS(a, name) RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* function = getExpr(where->expression);
  ASSERT_NE(function, nullptr);
  EXPECT_EQ(function->kind, GAST::GQLExpr::Kind::FunctionCall);
  EXPECT_EQ(function->text, "PROPERTY_EXISTS");
  ASSERT_EQ(function->children.size(), 2);

  const auto* element = getExpr(function->children[0]);
  const auto* property = getExpr(function->children[1]);
  ASSERT_NE(element, nullptr);
  ASSERT_NE(property, nullptr);
  EXPECT_EQ(element->text, "a");
  EXPECT_EQ(property->text, "name");
}

TEST(GQLParser, AllDifferentPredicateBecomesFunctionCall) {
  auto ast = parseGraphOrThrow("MATCH (a)-[e]->(b) WHERE ALL_DIFFERENT(a, b, e) RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);

  const auto* function = getExpr(where->expression);
  ASSERT_NE(function, nullptr);
  EXPECT_EQ(function->kind, GAST::GQLExpr::Kind::FunctionCall);
  EXPECT_EQ(function->text, "ALL_DIFFERENT");
  ASSERT_EQ(function->children.size(), 3);
}

TEST(GQLParser, ReturnClauseKeepsAliasAndTail) {
  auto ast = parseGraphOrThrow("MATCH (a:Person) RETURN a.name AS name, a.age ORDER BY a.age DESC OFFSET 2 LIMIT 5");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 3);

  const auto* return_clause = getReturnClause(*clauses);
  ASSERT_NE(return_clause, nullptr);
  EXPECT_FALSE(return_clause->distinct);
  ASSERT_EQ(return_clause->items.size(), 2);

  const auto* aliased = getExpr(return_clause->items[0]);
  ASSERT_NE(aliased, nullptr);
  EXPECT_EQ(aliased->tryGetAlias(), "name");

  const auto* page_clause = getPageClause(*clauses, 2);
  ASSERT_NE(page_clause, nullptr);

  const auto* order_by = page_clause->order_by->as<GAST::GQLOrderByClause>();
  ASSERT_NE(order_by, nullptr);
  ASSERT_EQ(order_by->items.size(), 1);

  const auto* order_item = order_by->items[0]->as<GAST::GQLOrderByItem>();
  ASSERT_NE(order_item, nullptr);
  EXPECT_TRUE(order_item->descending);

  const auto* offset = getExpr(page_clause->offset);
  const auto* limit = getExpr(page_clause->limit);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(limit, nullptr);
  EXPECT_EQ(offset->kind, GAST::GQLExpr::Kind::Literal);
  EXPECT_EQ(limit->kind, GAST::GQLExpr::Kind::Literal);
  EXPECT_EQ(offset->text, "2");
  EXPECT_EQ(limit->text, "5");
}

TEST(GQLParser, CompositeUnionQuery) {
  auto ast = parseGraphOrThrow("MATCH (a) RETURN a UNION MATCH (b) RETURN b");
  const auto* combined_query = ast->as<GAST::GQLCombinedQuery>();
  ASSERT_NE(combined_query, nullptr);
  ASSERT_EQ(combined_query->queries.size(), 2);
  ASSERT_EQ(combined_query->operators.size(), 1);
  EXPECT_EQ(combined_query->operators[0], GAST::CombinedQueryOperator::UnionDistinct);
  EXPECT_NE(combined_query->queries[0].get(), nullptr);
  EXPECT_NE(combined_query->queries[1].get(), nullptr);
}

TEST(GQLParser, CompositeUnionQueryKeepsOperatorSequence) {
  auto ast = parseGraphOrThrow("MATCH (a) RETURN a UNION ALL MATCH (b) RETURN b EXCEPT MATCH (c) RETURN c");
  const auto* combined_query = ast->as<GAST::GQLCombinedQuery>();
  ASSERT_NE(combined_query, nullptr);
  ASSERT_EQ(combined_query->queries.size(), 3);
  ASSERT_EQ(combined_query->operators.size(), 2);
  EXPECT_EQ(combined_query->operators[0], GAST::CombinedQueryOperator::UnionAll);
  EXPECT_EQ(combined_query->operators[1], GAST::CombinedQueryOperator::ExceptDistinct);
  EXPECT_EQ(formatAST(*combined_query), "MATCH (a) RETURN a UNION ALL MATCH (b) RETURN b EXCEPT MATCH (c) RETURN c");
}

TEST(GQLParser, NestedQuerySpecificationKeepsWrapper) {
  auto ast = parseGraphOrThrow("{ MATCH (a) RETURN a }");
  const auto* subquery = getSubquery(ast);
  ASSERT_NE(subquery, nullptr);

  const auto* clauses = getClausesQuery(subquery->query);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);
  ASSERT_NE(getMatchClause(*clauses), nullptr);
  ASSERT_NE(getReturnClause(*clauses), nullptr);
  EXPECT_EQ(formatAST(*subquery), "{ MATCH (a) RETURN a }");
}

TEST(GQLParser, StandaloneOrderByStatementKeepsStructuredPageClause) {
  auto ast = parseGraphOrThrow("MATCH (a) ORDER BY a DESC LIMIT 3 RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 3);

  const auto* page = getPageClause(*clauses, 1);
  ASSERT_NE(page, nullptr);
  EXPECT_EQ(formatAST(*page), "ORDER BY a DESC LIMIT 3");
}

TEST(GQLParser, FinishStatementKeepsStructuredClause) {
  auto ast = parseGraphOrThrow("MATCH (a) FINISH");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  ASSERT_NE(getFinishClause(*clauses, 1), nullptr);
}

TEST(GQLParser, ReturnClauseKeepsGroupBy) {
  auto ast = parseGraphOrThrow("MATCH (a) RETURN a GROUP BY a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* return_clause = getReturnClause(*clauses);
  ASSERT_NE(return_clause, nullptr);

  const auto* group_by = getGroupByClause(return_clause->group_by);
  ASSERT_NE(group_by, nullptr);
  ASSERT_EQ(group_by->items.size(), 1);
  EXPECT_EQ(getExpr(group_by->items[0])->text, "a");
  EXPECT_EQ(formatAST(*return_clause), "RETURN a GROUP BY a");
}

TEST(GQLParser, ReturnClauseSupportsEmptyGroupingSet) {
  auto ast = parseGraphOrThrow("RETURN a GROUP BY ()");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 1);

  const auto* return_clause = getReturnClause(*clauses, 0);
  ASSERT_NE(return_clause, nullptr);

  const auto* group_by = getGroupByClause(return_clause->group_by);
  ASSERT_NE(group_by, nullptr);
  EXPECT_TRUE(group_by->empty_grouping_set);
  EXPECT_TRUE(group_by->items.empty());
  EXPECT_EQ(formatAST(*return_clause), "RETURN a GROUP BY ()");
}

TEST(GQLParser, InlineCallVariableScopeIsRejected) {
  try {
    (void)parseGraphOrThrow("CALL (x) { RETURN x } RETURN x");
    FAIL() << "Expected `DB::Exception`";
  } catch (const DB::Exception& e) {
    EXPECT_NE(e.message().find("inline call variable scope"), String::npos);
  }
}

TEST(GQLParser, MatchClauseKeepsPathModePrefix) {
  auto ast = parseGraphOrThrow("MATCH WALK (a)-[e]->(b) RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);

  const auto* path = getOnlyPathPattern(*match);
  ASSERT_NE(path, nullptr);
  const auto* prefix = getPathPatternPrefix(path->prefix);
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->path_mode, GAST::PathMode::Walk);
  EXPECT_EQ(prefix->search_kind, GAST::PathSearchKind::None);
  EXPECT_EQ(formatAST(*clauses), "MATCH WALK (a)-[e]->(b) RETURN a");
}

TEST(GQLParser, MatchClauseKeepsMatchModeAndYield) {
  auto ast = parseGraphOrThrow("MATCH REPEATABLE ELEMENTS (a)-[e]->(b) YIELD e RETURN e");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  EXPECT_EQ(match->match_mode, GAST::GraphMatchMode::RepeatableElements);
  ASSERT_EQ(match->yield_items.size(), 1);
  EXPECT_EQ(getExpr(match->yield_items[0])->text, "e");
  EXPECT_EQ(formatAST(*clauses), "MATCH REPEATABLE ELEMENTS (a)-[e]->(b) YIELD e RETURN e");
}

TEST(GQLParser, ParenthesizedPathPatternKeepsQuestionQuantifier) {
  auto ast = parseGraphOrThrow("MATCH ((a)-[e]->(b))? RETURN e");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  const auto* path = getOnlyPathPattern(*match);
  ASSERT_NE(path, nullptr);
  ASSERT_EQ(path->elements.size(), 1);

  const auto* parenthesized = getParenthesizedPathPattern(path->elements[0]);
  ASSERT_NE(parenthesized, nullptr);
  const auto* quantifier = getQuantifier(parenthesized->quantifier);
  ASSERT_NE(quantifier, nullptr);
  EXPECT_EQ(quantifier->kind, GAST::GQLQuantifier::Kind::Question);
  EXPECT_EQ(formatAST(*clauses), "MATCH ((a)-[e]->(b))? RETURN e");
}

TEST(GQLParser, OptionalMatchBlockKeepsStructuredOperand) {
  auto ast = parseGraphOrThrow("OPTIONAL { MATCH (a) MATCH (b) } RETURN *");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 2);

  const auto* match = getMatchClause(*clauses, 0);
  ASSERT_NE(match, nullptr);
  EXPECT_TRUE(match->optional);

  const auto* block = getMatchStatementBlock(match->optional_operand_block);
  ASSERT_NE(block, nullptr);
  ASSERT_EQ(block->matches.size(), 2);
  EXPECT_EQ(formatAST(*match), "OPTIONAL { MATCH (a) MATCH (b) }");
}

TEST(GQLParser, ExistsPredicateKeepsGraphPatternBlock) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE EXISTS { WALK (a)-[e]->(b) } RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);
  const auto* exists = getExpr(where->expression);
  ASSERT_NE(exists, nullptr);
  EXPECT_EQ(exists->kind, GAST::GQLExpr::Kind::UnaryOp);
  EXPECT_EQ(exists->text, "EXISTS ");
  ASSERT_EQ(exists->children.size(), 1);

  const auto* block = getGraphPatternBlock(exists->children[0]);
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->match_mode, GAST::GraphMatchMode::None);
  ASSERT_EQ(block->path_patterns.size(), 1);
  EXPECT_EQ(formatAST(*clauses), "MATCH (a) WHERE EXISTS { WALK (a)-[e]->(b) } RETURN a");
}

TEST(GQLParser, ExistsPredicateKeepsMatchStatementBlock) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE EXISTS { MATCH (a)-[e]->(b) } RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);
  const auto* exists = getExpr(where->expression);
  ASSERT_NE(exists, nullptr);
  ASSERT_EQ(exists->children.size(), 1);

  const auto* block = getMatchStatementBlock(exists->children[0]);
  ASSERT_NE(block, nullptr);
  ASSERT_EQ(block->matches.size(), 1);
  EXPECT_EQ(formatAST(*clauses), "MATCH (a) WHERE EXISTS { MATCH (a)-[e]->(b) } RETURN a");
}

TEST(GQLParser, ExistsPredicateKeepsNestedQuery) {
  auto ast = parseGraphOrThrow("MATCH (a) WHERE EXISTS { RETURN a } RETURN a");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);

  const auto* match = getMatchClause(*clauses);
  ASSERT_NE(match, nullptr);
  const auto* where = match->where->as<GAST::GQLWhereClause>();
  ASSERT_NE(where, nullptr);
  const auto* exists = getExpr(where->expression);
  ASSERT_NE(exists, nullptr);
  ASSERT_EQ(exists->children.size(), 1);
  ASSERT_NE(getSubquery(exists->children[0]), nullptr);
  EXPECT_EQ(formatAST(*clauses), "MATCH (a) WHERE EXISTS { RETURN a } RETURN a");
}

TEST(GQLParser, ValueFunctionsBuildStructuredExpressions) {
  auto ast = parseGraphOrThrow("RETURN ABS(-1), CHAR_LENGTH('x'), CARDINALITY([1, 2])");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 1);

  const auto* return_clause = getReturnClause(*clauses, 0);
  ASSERT_NE(return_clause, nullptr);
  ASSERT_EQ(return_clause->items.size(), 3);

  const auto* abs_expr = getExpr(return_clause->items[0]);
  const auto* length_expr = getExpr(return_clause->items[1]);
  const auto* cardinality_expr = getExpr(return_clause->items[2]);
  ASSERT_NE(abs_expr, nullptr);
  ASSERT_NE(length_expr, nullptr);
  ASSERT_NE(cardinality_expr, nullptr);
  EXPECT_EQ(abs_expr->kind, GAST::GQLExpr::Kind::FunctionCall);
  EXPECT_EQ(abs_expr->text, "ABS");
  EXPECT_EQ(length_expr->text, "CHAR_LENGTH");
  EXPECT_EQ(cardinality_expr->text, "CARDINALITY");
  ASSERT_EQ(cardinality_expr->children.size(), 1);
  ASSERT_NE(getListConstructor(cardinality_expr->children[0]), nullptr);
}

TEST(GQLParser, AggregateAndConstructorsBuildStructuredNodes) {
  auto ast = parseGraphOrThrow("RETURN COUNT(*), [1, 2], RECORD {name: 1}");
  const auto* clauses = getClausesQuery(ast);
  ASSERT_NE(clauses, nullptr);
  ASSERT_EQ(clauses->clauses.size(), 1);

  const auto* return_clause = getReturnClause(*clauses, 0);
  ASSERT_NE(return_clause, nullptr);
  ASSERT_EQ(return_clause->items.size(), 3);

  const auto* count = getExpr(return_clause->items[0]);
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->kind, GAST::GQLExpr::Kind::FunctionCall);
  EXPECT_EQ(count->text, "COUNT");
  ASSERT_EQ(count->children.size(), 1);
  EXPECT_EQ(getExpr(count->children[0])->text, "*");

  const auto* list = getListConstructor(return_clause->items[1]);
  ASSERT_NE(list, nullptr);
  ASSERT_EQ(list->items.size(), 2);

  const auto* record = getRecordConstructor(return_clause->items[2]);
  ASSERT_NE(record, nullptr);
  EXPECT_TRUE(record->explicit_record_keyword);
  ASSERT_EQ(record->fields.size(), 1);
  EXPECT_EQ(formatAST(*clauses), "RETURN COUNT(*), [1, 2], RECORD {name: 1}");
}

TEST(GQLParser, FormatNodePattern) {
  auto node = make_intrusive<GAST::GQLNodePattern>();
  node->variable = GAST::GQLExpr::identifier("n");
  node->label_expression = GAST::GQLLabelExpression::name("Person");
  attachChildIfPresent(*node, node->variable);
  attachChildIfPresent(*node, node->label_expression);

  EXPECT_EQ(formatAST(*node), "(n:Person)");
}

TEST(GQLParser, FormatEdgePatternWithQuantifier) {
  auto edge = make_intrusive<GAST::GQLEdgePattern>(GAST::EdgeDirection::Right);
  edge->variable = GAST::GQLExpr::identifier("e");
  edge->label_expression = GAST::GQLLabelExpression::name("KNOWS");
  edge->quantifier = GAST::Ptr(make_intrusive<GAST::GQLQuantifier>(GAST::GQLQuantifier::Kind::Plus));
  attachChildIfPresent(*edge, edge->variable);
  attachChildIfPresent(*edge, edge->label_expression);
  attachChildIfPresent(*edge, edge->quantifier);

  EXPECT_EQ(formatAST(*edge), "-[e:KNOWS]->+");
}

TEST(GQLParser, CloneEdgeWithQuantifier) {
  auto edge = make_intrusive<GAST::GQLEdgePattern>(GAST::EdgeDirection::Right);
  edge->variable = GAST::GQLExpr::identifier("e");
  edge->label_expression = GAST::GQLLabelExpression::name("KNOWS");
  edge->quantifier = GAST::Ptr(make_intrusive<GAST::GQLQuantifier>(GAST::GQLQuantifier::Kind::Range, "1", "5"));
  attachChildIfPresent(*edge, edge->variable);
  attachChildIfPresent(*edge, edge->label_expression);
  attachChildIfPresent(*edge, edge->quantifier);

  auto cloned = edge->clone();
  const auto* cloned_edge = cloned->as<GAST::GQLEdgePattern>();
  ASSERT_NE(cloned_edge, nullptr);
  EXPECT_EQ(cloned_edge->direction, GAST::EdgeDirection::Right);
  EXPECT_NE(cloned_edge->variable.get(), edge->variable.get());
  EXPECT_NE(cloned_edge->quantifier.get(), edge->quantifier.get());
  EXPECT_EQ(formatAST(*cloned_edge), "-[e:KNOWS]->{1, 5}");
}

TEST(GQLParser, InvalidSyntaxThrowsException) {
  try {
    (void)parseGraphOrThrow("MATCH INVALID SYNTAX !!!");
    FAIL() << "Expected `DB::Exception`";
  } catch (const DB::Exception& e) {
    EXPECT_FALSE(e.message().empty());
  }
}

#endif
