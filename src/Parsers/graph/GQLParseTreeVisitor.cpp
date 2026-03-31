#include <Common/Exception.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>
#include <Parsers/graph/GraphAST.h>

namespace DB {

namespace ErrorCodes {
extern const int SYNTAX_ERROR;
}

namespace OPENGQL {

namespace {

using namespace AST;

String getText(antlr4::ParserRuleContext *context) { return context ? context->getText() : String{}; }

template <typename T>
T castAny(const std::any &value) {
  return std::any_cast<T>(value);
}

[[noreturn]] void throwUnsupported(const String &feature, antlr4::ParserRuleContext *context) {
  throw Exception(ErrorCodes::SYNTAX_ERROR, "Unsupported GQL {} in the current graph AST visitor: {}", feature, getText(context));
}

struct PatternBindingTable {
  PtrList path_patterns;
  Ptr where;
};

struct ElementPatternParts {
  Ptr variable;
  Ptr label_expression;
  Ptr properties;
  Ptr where;
};

struct ResultTail {
  Ptr order_by;
  Ptr offset;
  Ptr limit;
};

SetOperation getSetOperation(GQLParser::CompositeQueryExpressionContext *context) {
  if (auto *conjunction = context->queryConjunction()) {
    if (auto *set_operator = conjunction->setOperator()) {
      if (set_operator->UNION()) return SetOperation::Union;

      if (set_operator->EXCEPT()) return SetOperation::Except;

      if (set_operator->INTERSECT()) return SetOperation::Intersect;
    }

    if (conjunction->OTHERWISE()) return SetOperation::Otherwise;
  }

  return SetOperation::Union;
}

Ptr makeQuantifier(GQLParser::GraphPatternQuantifierContext *context) {
  if (!context) return {};

  if (context->ASTERISK()) return Ptr(make_intrusive<GQLQuantifier>(GQLQuantifier::Kind::Star));

  if (context->PLUS_SIGN()) return Ptr(make_intrusive<GQLQuantifier>(GQLQuantifier::Kind::Plus));

  if (auto *fixed = context->fixedQuantifier())
    return Ptr(make_intrusive<GQLQuantifier>(GQLQuantifier::Kind::Exact, getText(fixed->unsignedInteger())));

  if (auto *general = context->generalQuantifier()) {
    return Ptr(make_intrusive<GQLQuantifier>(GQLQuantifier::Kind::Range,
                                             general->lowerBound() ? getText(general->lowerBound()->unsignedInteger()) : String{},
                                             general->upperBound() ? getText(general->upperBound()->unsignedInteger()) : String{}));
  }

  return {};
}

Ptr makeQuestionQuantifier() { return Ptr(make_intrusive<GQLQuantifier>(GQLQuantifier::Kind::Question)); }

Ptr makeRawTextClause(const String & text)
{
  return GQLExpr::rawText(text);
}

Ptr makeUseGraphClause(GQLParser::UseGraphClauseContext * context)
{
  return Ptr(make_intrusive<GQLUseClause>(makeRawTextClause(getText(context->graphExpression()))));
}

void appendClause(PtrList & clauses, Ptr clause)
{
  if (clause)
    clauses.push_back(std::move(clause));
}

Ptr makeSelectSourceItem(Ptr graph_reference, Ptr source)
{
  auto item = make_intrusive<GQLSelectSourceItem>();
  item->graph_reference = std::move(graph_reference);
  item->source = std::move(source);
  appendClause(item->children, item->graph_reference);
  appendClause(item->children, item->source);
  return Ptr(item);
}

void appendClauseList(PtrList & clauses, PtrList && extra_clauses)
{
  for (auto & clause : extra_clauses)
    appendClause(clauses, std::move(clause));
}

void appendQueryResult(PtrList & clauses, Ptr query)
{
  if (!query)
    return;

  if (auto * clauses_query = query->as<GQLClausesQuery>())
  {
    for (auto & clause : clauses_query->clauses)
      appendClause(clauses, std::move(clause));

    clauses_query->children.clear();
    clauses_query->clauses.clear();
    return;
  }

  appendClause(clauses, std::move(query));
}

Ptr makeOrderByAndPageClause(ResultTail && tail)
{
  auto clause = make_intrusive<GQLPageClause>();
  clause->order_by = std::move(tail.order_by);
  clause->offset = std::move(tail.offset);
  clause->limit = std::move(tail.limit);

  appendClause(clause->children, clause->order_by);
  appendClause(clause->children, clause->offset);
  appendClause(clause->children, clause->limit);

  return Ptr(clause);
}

Ptr makeNodeOrEdgePattern(ElementPatternParts &&parts, bool is_edge, EdgeDirection direction = EdgeDirection::Right) {
  if (is_edge) {
    auto edge = make_intrusive<GQLEdgePattern>(direction);
    edge->variable = std::move(parts.variable);
    edge->label_expression = std::move(parts.label_expression);
    edge->properties = std::move(parts.properties);
    edge->where = std::move(parts.where);

    if (edge->variable) {
      edge->children.push_back(edge->variable);
    }
    if (edge->label_expression) {
      edge->children.push_back(edge->label_expression);
    }
    if (edge->properties) {
      edge->children.push_back(edge->properties);
    }
    if (edge->where) {
      edge->children.push_back(edge->where);
    }

    return Ptr(edge);
  }

  auto node = make_intrusive<GQLNodePattern>();
  node->variable = std::move(parts.variable);
  node->label_expression = std::move(parts.label_expression);
  node->properties = std::move(parts.properties);
  node->where = std::move(parts.where);

  if (node->variable) {
    node->children.push_back(node->variable);
  }
  if (node->label_expression) {
    node->children.push_back(node->label_expression);
  }
  if (node->properties) {
    node->children.push_back(node->properties);
  }
  if (node->where) {
    node->children.push_back(node->where);
  }

  return Ptr(node);
}

}  // namespace

Ptr GQLParseTreeVisitor::makeRawTextExpr(antlr4::ParserRuleContext *context) const { return GQLExpr::rawText(getText(context)); }

std::any GQLParseTreeVisitor::visitCompositeQueryStatement(GQLParser::CompositeQueryStatementContext *context) {
  return castAny<Ptr>(visit(context->compositeQueryExpression()));
}

std::any GQLParseTreeVisitor::visitCompositeQueryExpression(GQLParser::CompositeQueryExpressionContext *context) {
  if (!context->compositeQueryExpression()) {
    return castAny<Ptr>(visit(context->compositeQueryPrimary()));
  }

  auto left = castAny<Ptr>(visit(context->compositeQueryExpression()));
  auto right = castAny<Ptr>(visit(context->compositeQueryPrimary()));
  return Ptr(make_intrusive<GQLSetQuery>(getSetOperation(context), std::move(left), std::move(right)));
}

std::any GQLParseTreeVisitor::visitCompositeQueryPrimary(GQLParser::CompositeQueryPrimaryContext *context) {
  return castAny<Ptr>(visit(context->linearQueryStatement()));
}

std::any GQLParseTreeVisitor::visitNestedQuerySpecification(GQLParser::NestedQuerySpecificationContext *context)
{
  auto * procedure_body = context->procedureBody();

  if (!procedure_body || procedure_body->atSchemaClause() || procedure_body->bindingVariableDefinitionBlock() || !procedure_body->statementBlock())
    return makeRawTextExpr(context);

  auto * statement_block = procedure_body->statementBlock();

  if (!statement_block->statement() || !statement_block->nextStatement().empty())
    return makeRawTextExpr(context);

  auto * statement = statement_block->statement();

  if (!statement->compositeQueryStatement())
    return makeRawTextExpr(context);

  return castAny<Ptr>(visit(statement->compositeQueryStatement()));
}

std::any GQLParseTreeVisitor::visitFocusedNestedQuerySpecification(GQLParser::FocusedNestedQuerySpecificationContext *context)
{
  PtrList clauses;
  appendClause(clauses, makeUseGraphClause(context->useGraphClause()));
  appendQueryResult(clauses, castAny<Ptr>(visit(context->nestedQuerySpecification())));
  return Ptr(make_intrusive<GQLClausesQuery>(std::move(clauses)));
}

std::any GQLParseTreeVisitor::visitSelectStatement(GQLParser::SelectStatementContext *context)
{
  auto clause = make_intrusive<GQLProjectClause>(GQLProjectClause::Type::Select);

  if (context->setQuantifier() && context->setQuantifier()->DISTINCT())
    clause->distinct = true;

  if (context->ASTERISK())
  {
    clause->return_all = true;
  }
  else if (auto * items_context = context->selectItemList())
  {
    for (auto * item_context : items_context->selectItem())
    {
      auto expression = castAny<Ptr>(visit(item_context->aggregatingValueExpression()));

      if (item_context->selectItemAlias())
      {
        if (auto * with_alias = dynamic_cast<ASTWithAlias *>(expression.get()))
          with_alias->setAlias(getText(item_context->selectItemAlias()->identifier()));
        else
          throwUnsupported("select item alias on non-aliasable expression", item_context);
      }

      clause->items.push_back(expression);
      appendClause(clause->children, expression);
    }
  }

  if (auto * select_body = context->selectStatementBody())
  {
    if (auto * query_spec = select_body->selectQuerySpecification())
    {
      if (auto * nested_query = query_spec->nestedQuerySpecification())
      {
        auto source_query = castAny<Ptr>(visit(nested_query));

        if (query_spec->graphExpression())
          clause->source = makeSelectSourceItem(makeRawTextClause(getText(query_spec->graphExpression())), std::move(source_query));
        else
          clause->source = std::move(source_query);
      }
      else
      {
        clause->source = makeRawTextClause(getText(query_spec));
      }
    }
    else if (auto * match_list = select_body->selectGraphMatchList())
    {
      auto source_list = make_intrusive<GQLSelectSourceList>();

      for (auto * match_item : match_list->selectGraphMatch())
      {
        auto source_item = makeSelectSourceItem(
            makeRawTextClause(getText(match_item->graphExpression())),
            castAny<Ptr>(visit(match_item->matchStatement())));
        source_list->items.push_back(source_item);
        appendClause(source_list->children, source_item);
      }

      clause->source = Ptr(source_list);
    }

    appendClause(clause->children, clause->source);
  }

  if (context->whereClause())
  {
    clause->where = castAny<Ptr>(visit(context->whereClause()));
    appendClause(clause->children, clause->where);
  }

  if (context->groupByClause())
  {
    clause->group_by = castAny<Ptr>(visit(context->groupByClause()));
    appendClause(clause->children, clause->group_by);
  }

  if (context->havingClause())
  {
    clause->having = Ptr(make_intrusive<GQLWhereClause>(GQLWhereClause::Type::Having, castAny<Ptr>(visit(context->havingClause()->searchCondition()))));
    appendClause(clause->children, clause->having);
  }

  if (context->orderByClause())
  {
    clause->order_by = castAny<Ptr>(visit(context->orderByClause()));
    appendClause(clause->children, clause->order_by);
  }

  if (context->offsetClause())
  {
    clause->offset = castAny<Ptr>(visit(context->offsetClause()));
    appendClause(clause->children, clause->offset);
  }

  if (context->limitClause())
  {
    clause->limit = castAny<Ptr>(visit(context->limitClause()));
    appendClause(clause->children, clause->limit);
  }

  return Ptr(clause);
}

std::any GQLParseTreeVisitor::visitLinearQueryStatement(GQLParser::LinearQueryStatementContext *context) {
  if (context->ambientLinearQueryStatement()) {
    return castAny<Ptr>(visit(context->ambientLinearQueryStatement()));
  }

  auto * focused = context->focusedLinearQueryStatement();

  if (!focused)
    throwUnsupported("focused linear query statement", context);

  if (focused->selectStatement())
    return castAny<Ptr>(visit(focused->selectStatement()));

  if (focused->focusedNestedQuerySpecification())
    return castAny<Ptr>(visit(focused->focusedNestedQuerySpecification()));

  PtrList clauses;

  for (auto * part : focused->focusedLinearQueryStatementPart())
  {
    appendClause(clauses, makeUseGraphClause(part->useGraphClause()));
    appendClauseList(clauses, castAny<PtrList>(visit(part->simpleLinearQueryStatement())));
  }

  if (auto * part = focused->focusedLinearQueryAndPrimitiveResultStatementPart())
  {
    appendClause(clauses, makeUseGraphClause(part->useGraphClause()));
    appendClauseList(clauses, castAny<PtrList>(visit(part->simpleLinearQueryStatement())));
    appendClause(clauses, castAny<Ptr>(visit(part->primitiveResultStatement())));
  }
  else if (auto * part = focused->focusedPrimitiveResultStatement())
  {
    appendClause(clauses, makeUseGraphClause(part->useGraphClause()));
    appendClause(clauses, castAny<Ptr>(visit(part->primitiveResultStatement())));
  }

  if (!clauses.empty())
    return Ptr(make_intrusive<GQLClausesQuery>(std::move(clauses)));

  return makeRawTextExpr(focused);
}

std::any GQLParseTreeVisitor::visitAmbientLinearQueryStatement(GQLParser::AmbientLinearQueryStatementContext *context) {
  if (context->nestedQuerySpecification()) {
    return castAny<Ptr>(visit(context->nestedQuerySpecification()));
  }

  PtrList clauses;

  if (context->simpleLinearQueryStatement()) {
    auto items = castAny<PtrList>(visit(context->simpleLinearQueryStatement()));
    for (auto &item : items) {
      clauses.push_back(std::move(item));
    }
  }

  if (context->primitiveResultStatement()) {
    clauses.push_back(castAny<Ptr>(visit(context->primitiveResultStatement())));
  }

  return Ptr(make_intrusive<GQLClausesQuery>(std::move(clauses)));
}

std::any GQLParseTreeVisitor::visitSimpleLinearQueryStatement(GQLParser::SimpleLinearQueryStatementContext *context) {
  PtrList clauses;

  for (auto *statement : context->simpleQueryStatement()) {
    clauses.push_back(castAny<Ptr>(visit(statement)));
  }

  return clauses;
}

std::any GQLParseTreeVisitor::visitSimpleQueryStatement(GQLParser::SimpleQueryStatementContext *context) {
  if (context->primitiveQueryStatement()) {
    return castAny<Ptr>(visit(context->primitiveQueryStatement()));
  }

  return castAny<Ptr>(visit(context->callQueryStatement()));
}

std::any GQLParseTreeVisitor::visitCallQueryStatement(GQLParser::CallQueryStatementContext *context)
{
  auto clause = make_intrusive<GQLCallClause>();
  auto * statement = context->callProcedureStatement();
  clause->optional = statement->OPTIONAL() != nullptr;
  auto * procedure_call = statement->procedureCall();

  if (auto * named = procedure_call->namedProcedureCall())
  {
    clause->procedure = GQLExpr::rawText(getText(named->procedureReference()));

    if (auto * argument_list = named->procedureArgumentList())
    {
      for (auto * argument : argument_list->procedureArgument())
        clause->arguments.push_back(castAny<Ptr>(visit(argument->valueExpression())));
    }

    if (auto * yield_clause = named->yieldClause())
    {
      for (auto * item : yield_clause->yieldItemList()->yieldItem())
      {
        auto expression = GQLExpr::identifier(getText(item->yieldItemName()->fieldName()));

        if (item->yieldItemAlias())
        {
          if (auto * with_alias = dynamic_cast<ASTWithAlias *>(expression.get()))
            with_alias->setAlias(getText(item->yieldItemAlias()->bindingVariable()));
          else
            throwUnsupported("yield item alias on non-aliasable expression", item);
        }

        clause->yield_items.push_back(expression);
      }
    }
  }
  else if (auto * inline_call = procedure_call->inlineProcedureCall())
  {
    clause->inline_procedure = true;
    clause->procedure = GQLExpr::rawText(getText(inline_call));
  }

  appendClause(clause->children, clause->procedure);
  for (auto & argument : clause->arguments) appendClause(clause->children, argument);
  for (auto & item : clause->yield_items) appendClause(clause->children, item);
  return Ptr(clause);
}

std::any GQLParseTreeVisitor::visitPrimitiveQueryStatement(GQLParser::PrimitiveQueryStatementContext *context) {
  if (context->matchStatement()) {
    return castAny<Ptr>(visit(context->matchStatement()));
  }

  if (context->letStatement())
  {
    auto clause = make_intrusive<GQLLetClause>();

    for (auto * item : context->letStatement()->letVariableDefinitionList()->letVariableDefinition())
    {
      Ptr assignment;

      if (auto * value_definition = item->valueVariableDefinition())
      {
        Ptr value;
        String raw_type;

        if (auto * initializer = value_definition->optTypedValueInitializer())
        {
          if (initializer->valueType())
            raw_type = getText(initializer->valueType());

          if (initializer->valueInitializer())
            value = castAny<Ptr>(visit(initializer->valueInitializer()->valueExpression()));
        }

        assignment = Ptr(make_intrusive<GQLAssignmentItem>(getText(value_definition->bindingVariable()), std::move(value), true, std::move(raw_type)));
      }
      else
      {
        assignment = Ptr(make_intrusive<GQLAssignmentItem>(getText(item->bindingVariable()), castAny<Ptr>(visit(item->valueExpression()))));
      }

      clause->items.push_back(assignment);
      appendClause(clause->children, assignment);
    }

    return Ptr(clause);
  }

  if (context->forStatement())
  {
    auto clause = make_intrusive<GQLForClause>();
    auto * statement = context->forStatement();
    clause->alias = getText(statement->forItem()->forItemAlias()->bindingVariable());
    clause->source = castAny<Ptr>(visit(statement->forItem()->forItemSource()->valueExpression()));
    appendClause(clause->children, clause->source);

    if (auto * ordinality_or_offset = statement->forOrdinalityOrOffset())
    {
      clause->with_ordinality = ordinality_or_offset->ORDINALITY();
      clause->with_offset = ordinality_or_offset->OFFSET();
      clause->ordinality_or_offset_alias = getText(ordinality_or_offset->bindingVariable());
    }

    return Ptr(clause);
  }

  if (context->filterStatement()) {
    return castAny<Ptr>(visit(context->filterStatement()));
  }

  if (context->orderByAndPageStatement())
  {
    auto tail = castAny<ResultTail>(visit(context->orderByAndPageStatement()));
    return makeOrderByAndPageClause(std::move(tail));
  }

  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitMatchStatement(GQLParser::MatchStatementContext *context) {
  if (context->simpleMatchStatement()) {
    return castAny<Ptr>(visit(context->simpleMatchStatement()));
  }

  return castAny<Ptr>(visit(context->optionalMatchStatement()));
}

std::any GQLParseTreeVisitor::visitSimpleMatchStatement(GQLParser::SimpleMatchStatementContext *context) {
  auto binding_table = castAny<PatternBindingTable>(visit(context->graphPatternBindingTable()));
  auto clause = make_intrusive<GQLMatchClause>(false);
  clause->path_patterns = std::move(binding_table.path_patterns);
  clause->where = std::move(binding_table.where);

  for (const auto &path : clause->path_patterns) {
    if (path) {
      clause->children.push_back(path);
    }
  }

  if (clause->where) {
    clause->children.push_back(clause->where);
  }

  return Ptr(clause);
}

std::any GQLParseTreeVisitor::visitOptionalMatchStatement(GQLParser::OptionalMatchStatementContext *context) {
  if (!context->optionalOperand()->simpleMatchStatement()) {
    throwUnsupported("complex optional match operand", context);
  }

  auto clause = castAny<Ptr>(visit(context->optionalOperand()->simpleMatchStatement()));
  if (auto *match_clause = clause->as<GQLMatchClause>()) {
    match_clause->optional = true;
    return clause;
  }

  throwUnsupported("optional match clause", context);
}

std::any GQLParseTreeVisitor::visitFilterStatement(GQLParser::FilterStatementContext *context) {
  if (context->whereClause()) {
    return castAny<Ptr>(visit(context->whereClause()));
  }

  return Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(context->searchCondition()))));
}

std::any GQLParseTreeVisitor::visitWhereClause(GQLParser::WhereClauseContext *context) {
  return Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(context->searchCondition()))));
}

std::any GQLParseTreeVisitor::visitGraphPatternBindingTable(GQLParser::GraphPatternBindingTableContext *context) {
  if (context->graphPatternYieldClause()) {
    throwUnsupported("graph pattern `YIELD`", context);
  }

  return castAny<PatternBindingTable>(visit(context->graphPattern()));
}

std::any GQLParseTreeVisitor::visitGraphPattern(GQLParser::GraphPatternContext *context) {
  if (context->matchMode() || context->keepClause()) {
    throwUnsupported("graph pattern prefixes", context);
  }

  PatternBindingTable result;
  result.path_patterns = castAny<PtrList>(visit(context->pathPatternList()));

  if (context->graphPatternWhereClause()) {
    result.where = castAny<Ptr>(visit(context->graphPatternWhereClause()));
  }

  return result;
}

std::any GQLParseTreeVisitor::visitGraphPatternWhereClause(GQLParser::GraphPatternWhereClauseContext *context) {
  return Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(context->searchCondition()))));
}

std::any GQLParseTreeVisitor::visitPathPatternList(GQLParser::PathPatternListContext *context) {
  PtrList patterns;

  for (auto *pattern : context->pathPattern()) {
    patterns.push_back(castAny<Ptr>(visit(pattern)));
  }

  return patterns;
}

std::any GQLParseTreeVisitor::visitPathPattern(GQLParser::PathPatternContext *context) {
  if (context->pathPatternPrefix()) {
    throwUnsupported("path pattern prefix", context);
  }

  auto elements = castAny<PtrList>(visit(context->pathPatternExpression()));
  auto pattern = make_intrusive<GQLPathPattern>(std::move(elements));

  if (context->pathVariableDeclaration()) {
    pattern->variable = GQLExpr::identifier(getText(context->pathVariableDeclaration()->pathVariable()->bindingVariable()));
    pattern->children.insert(pattern->children.begin(), pattern->variable);
  }

  return Ptr(pattern);
}

std::any GQLParseTreeVisitor::visitPpePathTerm(GQLParser::PpePathTermContext *context) {
  return castAny<PtrList>(visit(context->pathTerm()));
}

std::any GQLParseTreeVisitor::visitPathTerm(GQLParser::PathTermContext *context) {
  PtrList elements;

  for (auto *factor : context->pathFactor()) {
    elements.push_back(castAny<Ptr>(visit(factor)));
  }

  return elements;
}

std::any GQLParseTreeVisitor::visitPfPathPrimary(GQLParser::PfPathPrimaryContext *context) {
  return castAny<Ptr>(visit(context->pathPrimary()));
}

std::any GQLParseTreeVisitor::visitPfQuantifiedPathPrimary(GQLParser::PfQuantifiedPathPrimaryContext *context) {
  auto node = castAny<Ptr>(visit(context->pathPrimary()));
  auto quantifier = makeQuantifier(context->graphPatternQuantifier());

  if (auto *edge = node->as<GQLEdgePattern>()) {
    edge->quantifier = std::move(quantifier);

    if (edge->quantifier) {
      edge->children.push_back(edge->quantifier);
    }

    return node;
  }

  throwUnsupported("quantified non-edge path primary", context);
}

std::any GQLParseTreeVisitor::visitPfQuestionedPathPrimary(GQLParser::PfQuestionedPathPrimaryContext *context) {
  auto node = castAny<Ptr>(visit(context->pathPrimary()));

  if (auto *edge = node->as<GQLEdgePattern>()) {
    edge->quantifier = makeQuestionQuantifier();
    edge->children.push_back(edge->quantifier);
    return node;
  }

  throwUnsupported("question-mark non-edge path primary", context);
}

std::any GQLParseTreeVisitor::visitPpElementPattern(GQLParser::PpElementPatternContext *context) {
  return castAny<Ptr>(visit(context->elementPattern()));
}

std::any GQLParseTreeVisitor::visitElementPattern(GQLParser::ElementPatternContext *context) {
  if (context->nodePattern()) {
    return castAny<Ptr>(visit(context->nodePattern()));
  }

  return castAny<Ptr>(visit(context->edgePattern()));
}

std::any GQLParseTreeVisitor::visitNodePattern(GQLParser::NodePatternContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), false);
}

std::any GQLParseTreeVisitor::visitElementPatternFiller(GQLParser::ElementPatternFillerContext *context) {
  ElementPatternParts parts;

  if (context->elementVariableDeclaration()) {
    parts.variable = GQLExpr::identifier(getText(context->elementVariableDeclaration()->elementVariable()->bindingVariable()));
  }

  if (context->isLabelExpression()) {
    parts.label_expression = castAny<Ptr>(visit(context->isLabelExpression()->labelExpression()));
  }

  if (context->elementPatternPredicate()) {
    if (context->elementPatternPredicate()->elementPropertySpecification()) {
      parts.properties = castAny<Ptr>(visit(context->elementPatternPredicate()->elementPropertySpecification()));
    } else if (context->elementPatternPredicate()->elementPatternWhereClause()) {
      parts.where = castAny<Ptr>(visit(context->elementPatternPredicate()->elementPatternWhereClause()));
    }
  }

  return parts;
}

std::any GQLParseTreeVisitor::visitElementPatternWhereClause(GQLParser::ElementPatternWhereClauseContext *context) {
  return Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(context->searchCondition()))));
}

std::any GQLParseTreeVisitor::visitElementPropertySpecification(GQLParser::ElementPropertySpecificationContext *context) {
  return Ptr(make_intrusive<GQLPropertyMap>(castAny<PtrList>(visit(context->propertyKeyValuePairList()))));
}

std::any GQLParseTreeVisitor::visitPropertyKeyValuePairList(GQLParser::PropertyKeyValuePairListContext *context) {
  PtrList items;

  for (auto *item : context->propertyKeyValuePair()) {
    items.push_back(castAny<Ptr>(visit(item)));
  }

  return items;
}

std::any GQLParseTreeVisitor::visitPropertyKeyValuePair(GQLParser::PropertyKeyValuePairContext *context) {
  return Ptr(
      make_intrusive<GQLPropertyItem>(getText(context->propertyName()->identifier()), castAny<Ptr>(visit(context->valueExpression()))));
}

std::any GQLParseTreeVisitor::visitEdgePattern(GQLParser::EdgePatternContext *context) {
  if (context->fullEdgePattern()) {
    return castAny<Ptr>(visit(context->fullEdgePattern()));
  }

  return castAny<Ptr>(visit(context->abbreviatedEdgePattern()));
}

std::any GQLParseTreeVisitor::visitFullEdgePattern(GQLParser::FullEdgePatternContext *context) {
  if (context->fullEdgePointingLeft()) {
    return castAny<Ptr>(visit(context->fullEdgePointingLeft()));
  }
  if (context->fullEdgeUndirected()) {
    return castAny<Ptr>(visit(context->fullEdgeUndirected()));
  }
  if (context->fullEdgePointingRight()) {
    return castAny<Ptr>(visit(context->fullEdgePointingRight()));
  }
  if (context->fullEdgeLeftOrUndirected()) {
    return castAny<Ptr>(visit(context->fullEdgeLeftOrUndirected()));
  }
  if (context->fullEdgeUndirectedOrRight()) {
    return castAny<Ptr>(visit(context->fullEdgeUndirectedOrRight()));
  }
  if (context->fullEdgeLeftOrRight()) {
    return castAny<Ptr>(visit(context->fullEdgeLeftOrRight()));
  }

  return castAny<Ptr>(visit(context->fullEdgeAnyDirection()));
}

std::any GQLParseTreeVisitor::visitFullEdgePointingLeft(GQLParser::FullEdgePointingLeftContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::Left);
}

std::any GQLParseTreeVisitor::visitFullEdgeUndirected(GQLParser::FullEdgeUndirectedContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::Undirected);
}

std::any GQLParseTreeVisitor::visitFullEdgePointingRight(GQLParser::FullEdgePointingRightContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::Right);
}

std::any GQLParseTreeVisitor::visitFullEdgeLeftOrUndirected(GQLParser::FullEdgeLeftOrUndirectedContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::LeftOrUndirected);
}

std::any GQLParseTreeVisitor::visitFullEdgeUndirectedOrRight(GQLParser::FullEdgeUndirectedOrRightContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true,
                               EdgeDirection::UndirectedOrRight);
}

std::any GQLParseTreeVisitor::visitFullEdgeLeftOrRight(GQLParser::FullEdgeLeftOrRightContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::LeftOrRight);
}

std::any GQLParseTreeVisitor::visitFullEdgeAnyDirection(GQLParser::FullEdgeAnyDirectionContext *context) {
  return makeNodeOrEdgePattern(castAny<ElementPatternParts>(visit(context->elementPatternFiller())), true, EdgeDirection::Any);
}

std::any GQLParseTreeVisitor::visitAbbreviatedEdgePattern(GQLParser::AbbreviatedEdgePatternContext *context) {
  EdgeDirection direction = EdgeDirection::Any;

  if (context->LEFT_ARROW()) {
    direction = EdgeDirection::Left;
  } else if (context->RIGHT_ARROW()) {
    direction = EdgeDirection::Right;
  } else if (context->TILDE()) {
    direction = EdgeDirection::Undirected;
  } else if (context->LEFT_ARROW_TILDE()) {
    direction = EdgeDirection::LeftOrUndirected;
  } else if (context->TILDE_RIGHT_ARROW()) {
    direction = EdgeDirection::UndirectedOrRight;
  } else if (context->LEFT_MINUS_RIGHT()) {
    direction = EdgeDirection::LeftOrRight;
  }

  return makeNodeOrEdgePattern({}, true, direction);
}

std::any GQLParseTreeVisitor::visitLabelExpressionNegation(GQLParser::LabelExpressionNegationContext *context) {
  return GQLLabelExpression::unary("!", castAny<Ptr>(visit(context->labelExpression())));
}

std::any GQLParseTreeVisitor::visitLabelExpressionDisjunction(GQLParser::LabelExpressionDisjunctionContext *context) {
  return GQLLabelExpression::binary(GQLLabelExpression::Kind::Disjunction, castAny<Ptr>(visit(context->labelExpression(0))),
                                    castAny<Ptr>(visit(context->labelExpression(1))));
}

std::any GQLParseTreeVisitor::visitLabelExpressionConjunction(GQLParser::LabelExpressionConjunctionContext *context) {
  return GQLLabelExpression::binary(GQLLabelExpression::Kind::Conjunction, castAny<Ptr>(visit(context->labelExpression(0))),
                                    castAny<Ptr>(visit(context->labelExpression(1))));
}

std::any GQLParseTreeVisitor::visitLabelExpressionName(GQLParser::LabelExpressionNameContext *context) {
  return GQLLabelExpression::name(getText(context->labelName()->identifier()));
}

std::any GQLParseTreeVisitor::visitLabelExpressionWildcard(GQLParser::LabelExpressionWildcardContext *) {
  return GQLLabelExpression::wildcard();
}

std::any GQLParseTreeVisitor::visitLabelExpressionParenthesized(GQLParser::LabelExpressionParenthesizedContext *context) {
  return castAny<Ptr>(visit(context->labelExpression()));
}

std::any GQLParseTreeVisitor::visitPrimitiveResultStatement(GQLParser::PrimitiveResultStatementContext *context) {
  if (context->FINISH()) {
    return Ptr(make_intrusive<GQLFinishClause>());
  }

  auto clause = castAny<Ptr>(visit(context->returnStatement()));

  if (context->orderByAndPageStatement()) {
    auto tail = castAny<ResultTail>(visit(context->orderByAndPageStatement()));

    if (auto *project = clause->as<GQLProjectClause>()) {
      project->order_by = std::move(tail.order_by);
      project->offset = std::move(tail.offset);
      project->limit = std::move(tail.limit);

      if (project->order_by) {
        project->children.push_back(project->order_by);
      }
      if (project->offset) {
        project->children.push_back(project->offset);
      }
      if (project->limit) {
        project->children.push_back(project->limit);
      }
    }
  }

  return clause;
}

std::any GQLParseTreeVisitor::visitReturnStatement(GQLParser::ReturnStatementContext *context) {
  return castAny<Ptr>(visit(context->returnStatementBody()));
}

std::any GQLParseTreeVisitor::visitReturnStatementBody(GQLParser::ReturnStatementBodyContext *context) {
  auto clause = make_intrusive<GQLProjectClause>();

  if (context->setQuantifier() && context->setQuantifier()->DISTINCT()) {
    clause->distinct = true;
  }

  if (context->ASTERISK()) {
    clause->return_all = true;
  } else if (context->returnItemList()) {
    clause->items = castAny<PtrList>(visit(context->returnItemList()));

    for (const auto &item : clause->items) {
      if (item) {
        clause->children.push_back(item);
      }
    }
  }

  if (context->groupByClause())
  {
    clause->group_by = castAny<Ptr>(visit(context->groupByClause()));

    if (clause->group_by)
      clause->children.push_back(clause->group_by);
  }

  return Ptr(clause);
}

std::any GQLParseTreeVisitor::visitReturnItemList(GQLParser::ReturnItemListContext *context) {
  PtrList items;

  for (auto *item : context->returnItem()) {
    items.push_back(castAny<Ptr>(visit(item)));
  }

  return items;
}

std::any GQLParseTreeVisitor::visitReturnItem(GQLParser::ReturnItemContext *context) {
  auto expression = castAny<Ptr>(visit(context->aggregatingValueExpression()));

  if (context->returnItemAlias()) {
    if (auto *with_alias = dynamic_cast<ASTWithAlias *>(expression.get())) {
      with_alias->setAlias(getText(context->returnItemAlias()->identifier()));
    } else {
      throwUnsupported("return item alias on non-aliasable expression", context);
    }
  }

  return expression;
}

std::any GQLParseTreeVisitor::visitGroupByClause(GQLParser::GroupByClauseContext *context)
{
  String text = "GROUP BY";

  if (auto * grouping_list = context->groupingElementList())
  {
    if (grouping_list->emptyGroupingSet())
    {
      text += " ()";
    }
    else
    {
      bool first = true;

      for (auto * element : grouping_list->groupingElement())
      {
        text += first ? " " : ", ";
        text += getText(element->bindingVariableReference()->bindingVariable());
        first = false;
      }
    }
  }

  return makeRawTextClause(text);
}

std::any GQLParseTreeVisitor::visitOrderByAndPageStatement(GQLParser::OrderByAndPageStatementContext *context) {
  ResultTail result;

  if (context->orderByClause()) {
    result.order_by = castAny<Ptr>(visit(context->orderByClause()));
  }

  if (context->offsetClause()) {
    result.offset = castAny<Ptr>(visit(context->offsetClause()));
  }

  if (context->limitClause()) {
    result.limit = castAny<Ptr>(visit(context->limitClause()));
  }

  return result;
}

std::any GQLParseTreeVisitor::visitOrderByClause(GQLParser::OrderByClauseContext *context) {
  return Ptr(make_intrusive<GQLOrderByClause>(castAny<PtrList>(visit(context->sortSpecificationList()))));
}

std::any GQLParseTreeVisitor::visitSortSpecificationList(GQLParser::SortSpecificationListContext *context) {
  PtrList items;

  for (auto *item : context->sortSpecification()) {
    items.push_back(castAny<Ptr>(visit(item)));
  }

  return items;
}

std::any GQLParseTreeVisitor::visitSortSpecification(GQLParser::SortSpecificationContext *context) {
  const bool descending =
      context->orderingSpecification() && (context->orderingSpecification()->DESC() || context->orderingSpecification()->DESCENDING());

  return Ptr(make_intrusive<GQLOrderByItem>(castAny<Ptr>(visit(context->sortKey()->aggregatingValueExpression())), descending));
}

std::any GQLParseTreeVisitor::visitLimitClause(GQLParser::LimitClauseContext *context) {
  return GQLExpr::literal(getText(context->nonNegativeIntegerSpecification()));
}

std::any GQLParseTreeVisitor::visitOffsetClause(GQLParser::OffsetClauseContext *context) {
  return GQLExpr::literal(getText(context->nonNegativeIntegerSpecification()));
}

std::any GQLParseTreeVisitor::visitSearchCondition(GQLParser::SearchConditionContext *context) {
  return castAny<Ptr>(visit(context->booleanValueExpression()->valueExpression()));
}

std::any GQLParseTreeVisitor::visitAggregatingValueExpression(GQLParser::AggregatingValueExpressionContext *context) {
  return castAny<Ptr>(visit(context->valueExpression()));
}

std::any GQLParseTreeVisitor::visitConjunctiveExprAlt(GQLParser::ConjunctiveExprAltContext *context) {
  return GQLExpr::binaryOp("AND", castAny<Ptr>(visit(context->valueExpression(0))), castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitMultDivExprAlt(GQLParser::MultDivExprAltContext *context) {
  return GQLExpr::binaryOp(context->operator_ ? context->operator_->getText() : String("/"),
                           castAny<Ptr>(visit(context->valueExpression(0))), castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitSignedExprAlt(GQLParser::SignedExprAltContext *context) {
  if (context->PLUS_SIGN()) {
    return castAny<Ptr>(visit(context->valueExpression()));
  }

  return GQLExpr::unaryOp("-", castAny<Ptr>(visit(context->valueExpression())));
}

std::any GQLParseTreeVisitor::visitIsNotExprAlt(GQLParser::IsNotExprAltContext *context)
{
  const String op = context->NOT() ? "IS NOT" : "IS";
  return GQLExpr::binaryOp(op, castAny<Ptr>(visit(context->valueExpression())), GQLExpr::literal(getText(context->truthValue())));
}

std::any GQLParseTreeVisitor::visitNotExprAlt(GQLParser::NotExprAltContext *context) {
  return GQLExpr::unaryOp("NOT ", castAny<Ptr>(visit(context->valueExpression())));
}

std::any GQLParseTreeVisitor::visitValueFunctionExprAlt(GQLParser::ValueFunctionExprAltContext *context) {
  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitConcatenationExprAlt(GQLParser::ConcatenationExprAltContext *context) {
  return GQLExpr::binaryOp("||", castAny<Ptr>(visit(context->valueExpression(0))), castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitDisjunctiveExprAlt(GQLParser::DisjunctiveExprAltContext *context) {
  return GQLExpr::binaryOp(context->OR() ? "OR" : "XOR", castAny<Ptr>(visit(context->valueExpression(0))),
                           castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitComparisonExprAlt(GQLParser::ComparisonExprAltContext *context) {
  return GQLExpr::binaryOp(getText(context->compOp()), castAny<Ptr>(visit(context->valueExpression(0))),
                           castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitPrimaryExprAlt(GQLParser::PrimaryExprAltContext *context) {
  return castAny<Ptr>(visit(context->valueExpressionPrimary()));
}

std::any GQLParseTreeVisitor::visitAddSubtractExprAlt(GQLParser::AddSubtractExprAltContext *context) {
  return GQLExpr::binaryOp(context->PLUS_SIGN() ? "+" : "-", castAny<Ptr>(visit(context->valueExpression(0))),
                           castAny<Ptr>(visit(context->valueExpression(1))));
}

std::any GQLParseTreeVisitor::visitPredicateExprAlt(GQLParser::PredicateExprAltContext *context)
{
  auto * predicate = context->predicate();

  if (auto * null_predicate = predicate->nullPredicate())
  {
    const String op = null_predicate->nullPredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, castAny<Ptr>(visit(null_predicate->valueExpressionPrimary())), GQLExpr::literal("NULL"));
  }

  if (auto * value_type_predicate = predicate->valueTypePredicate())
  {
    const String op = value_type_predicate->valueTypePredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(
        op,
        castAny<Ptr>(visit(value_type_predicate->valueExpressionPrimary())),
        GQLExpr::rawText(getText(value_type_predicate->valueTypePredicatePart2()->valueType())));
  }

  if (auto * directed_predicate = predicate->directedPredicate())
  {
    const String op = directed_predicate->directedPredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, GQLExpr::identifier(getText(directed_predicate->elementVariableReference()->bindingVariable())),
                             GQLExpr::literal("DIRECTED"));
  }

  if (auto * labeled_predicate = predicate->labeledPredicate())
  {
    auto * label_part = labeled_predicate->labeledPredicatePart2()->isLabeledOrColon();
    String op = ":";

    if (label_part->IS())
      op = label_part->NOT() ? "IS NOT LABELED" : "IS LABELED";

    return GQLExpr::binaryOp(op, GQLExpr::identifier(getText(labeled_predicate->elementVariableReference()->bindingVariable())),
                             castAny<Ptr>(visit(labeled_predicate->labeledPredicatePart2()->labelExpression())));
  }

  if (auto * source_destination_predicate = predicate->sourceDestinationPredicate())
  {
    auto left = GQLExpr::identifier(getText(source_destination_predicate->nodeReference()->elementVariableReference()->bindingVariable()));

    if (auto * source = source_destination_predicate->sourcePredicatePart2())
    {
      const String op = source->NOT() ? "IS NOT SOURCE OF" : "IS SOURCE OF";
      auto right = GQLExpr::identifier(getText(source->edgeReference()->elementVariableReference()->bindingVariable()));
      return GQLExpr::binaryOp(op, left, right);
    }

    auto * destination = source_destination_predicate->destinationPredicatePart2();
    const String op = destination->NOT() ? "IS NOT DESTINATION OF" : "IS DESTINATION OF";
    auto right = GQLExpr::identifier(getText(destination->edgeReference()->elementVariableReference()->bindingVariable()));
    return GQLExpr::binaryOp(op, left, right);
  }

  if (auto * all_different_predicate = predicate->all_differentPredicate())
  {
    PtrList arguments;

    for (auto * reference : all_different_predicate->elementVariableReference())
      arguments.push_back(GQLExpr::identifier(getText(reference->bindingVariable())));

    return GQLExpr::functionCall("ALL_DIFFERENT", std::move(arguments));
  }

  if (auto * same_predicate = predicate->samePredicate())
  {
    PtrList arguments;

    for (auto * reference : same_predicate->elementVariableReference())
      arguments.push_back(GQLExpr::identifier(getText(reference->bindingVariable())));

    return GQLExpr::functionCall("SAME", std::move(arguments));
  }

  if (auto * property_exists_predicate = predicate->property_existsPredicate())
  {
    PtrList arguments;
    arguments.push_back(GQLExpr::identifier(getText(property_exists_predicate->elementVariableReference()->bindingVariable())));
    arguments.push_back(GQLExpr::literal(getText(property_exists_predicate->propertyName()->identifier())));
    return GQLExpr::functionCall("PROPERTY_EXISTS", std::move(arguments));
  }

  if (auto * exists_predicate = predicate->existsPredicate())
  {
    String body;

    if (exists_predicate->graphPattern())
      body = "{" + getText(exists_predicate->graphPattern()) + "}";
    else if (exists_predicate->matchStatementBlock())
      body = (exists_predicate->LEFT_PAREN() ? "(" : "{") + getText(exists_predicate->matchStatementBlock())
          + (exists_predicate->RIGHT_PAREN() ? ")" : "}");
    else if (exists_predicate->nestedQuerySpecification())
      body = getText(exists_predicate->nestedQuerySpecification());

    return GQLExpr::unaryOp("EXISTS ", GQLExpr::rawText(body));
  }

  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitValueExpressionPrimary(GQLParser::ValueExpressionPrimaryContext *context) {
  if (context->bindingVariableReference()) {
    return GQLExpr::identifier(getText(context->bindingVariableReference()->bindingVariable()));
  }

  if (context->unsignedValueSpecification()) {
    return castAny<Ptr>(visit(context->unsignedValueSpecification()));
  }

  if (context->valueExpressionPrimary() && context->propertyName()) {
    return GQLExpr::property(castAny<Ptr>(visit(context->valueExpressionPrimary())), getText(context->propertyName()->identifier()));
  }

  if (context->parenthesizedValueExpression()) {
    return castAny<Ptr>(visit(context->parenthesizedValueExpression()->valueExpression()));
  }

  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitUnsignedValueSpecification(GQLParser::UnsignedValueSpecificationContext *context) {
  if (context->unsignedLiteral()) {
    return castAny<Ptr>(visit(context->unsignedLiteral()));
  }

  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitUnsignedLiteral(GQLParser::UnsignedLiteralContext *context) {
  return GQLExpr::literal(getText(context));
}

}  // namespace OPENGQL

}  // namespace DB
