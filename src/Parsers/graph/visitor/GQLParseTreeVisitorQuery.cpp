#include <Parsers/graph/visitor/GQLVisitorCommon.h>

namespace DB {

namespace OPENGQL {

Ptr makeSelectSourceItem(Ptr graph_reference, Ptr source) {
  auto item = make_intrusive<GQLSelectSourceItem>();
  item->graph_reference = std::move(graph_reference);
  item->source = std::move(source);
  appendClause(item->children, item->graph_reference);
  appendClause(item->children, item->source);
  return Ptr(item);
}

Ptr GQLParseTreeVisitor::buildSubquery(GQLParser::ProcedureBodyContext *procedure_body, antlr4::ParserRuleContext *context) {
  auto *statement_block = procedure_body ? procedure_body->statementBlock() : nullptr;

  if (!procedure_body || !statement_block || !statement_block->statement()) {
    throwUnsupported("nested query specification body", context);
  }

  auto visit_statement = [this](GQLParser::StatementContext *statement) -> Ptr {
    if (!statement) return {};

    if (statement->compositeQueryStatement()) return castAny<Ptr>(visit(statement->compositeQueryStatement()));

    if (statement->linearDataModifyingStatement()) return makeLinearDataModifyingQuery(statement->linearDataModifyingStatement(), *this);

    if (statement->linearCatalogModifyingStatement()) return castAny<Ptr>(visit(statement->linearCatalogModifyingStatement()));

    throwUnsupported("nested non-query statement", statement);
  };

  auto make_binding_initializer = [this](antlr4::ParserRuleContext *initializer_context, antlr4::ParserRuleContext *expression_context,
                                         GQLBindingInitializer::Kind kind) -> Ptr {
    if (!initializer_context || !expression_context) {
      throwUnsupported("binding variable initializer", initializer_context);
    }

    Ptr initializer_value;

    if (auto *graph_expression = dynamic_cast<GQLParser::GraphExpressionContext *>(expression_context)) {
      initializer_value = makeGraphExpression(graph_expression, *this);
    } else if (auto *binding_table_expression = dynamic_cast<GQLParser::BindingTableExpressionContext *>(expression_context)) {
      initializer_value = makeBindingTableExpression(binding_table_expression, *this);
    } else if (auto *value_expression = dynamic_cast<GQLParser::ValueExpressionContext *>(expression_context)) {
      initializer_value = castAny<Ptr>(visit(value_expression));
    } else {
      throwUnsupported("binding variable initializer expression", expression_context);
    }

    return makeBindingInitializer(kind, std::move(initializer_value));
  };

  auto make_binding_definition = [&](GQLParser::BindingVariableDefinitionContext *binding_context) -> Ptr {
    if (!binding_context) {
      return {};
    }

    if (auto *graph_definition = binding_context->graphVariableDefinition()) {
      auto definition = make_intrusive<GQLBindingVariableDefinition>(GQLBindingVariableDefinition::Kind::Graph);
      definition->property_keyword = graph_definition->PROPERTY() != nullptr;
      definition->name = getText(graph_definition->bindingVariable());
      auto *initializer = graph_definition->optTypedGraphInitializer();
      definition->raw_type =
          getTypedRawType(initializer ? initializer->typed() : nullptr, initializer ? initializer->graphReferenceValueType() : nullptr);
      definition->initializer = make_binding_initializer(initializer ? initializer->graphInitializer() : nullptr,
                                                         initializer ? initializer->graphInitializer()->graphExpression() : nullptr,
                                                         GQLBindingInitializer::Kind::Graph);
      appendClause(definition->children, definition->initializer);
      return Ptr(definition);
    }

    if (auto *binding_table_definition = binding_context->bindingTableVariableDefinition()) {
      auto definition = make_intrusive<GQLBindingVariableDefinition>(GQLBindingVariableDefinition::Kind::BindingTable);
      definition->binding_keyword = binding_table_definition->BINDING() != nullptr;
      definition->name = getText(binding_table_definition->bindingVariable());
      auto *initializer = binding_table_definition->optTypedBindingTableInitializer();
      definition->raw_type = getTypedRawType(initializer ? initializer->typed() : nullptr,
                                             initializer ? initializer->bindingTableReferenceValueType() : nullptr);
      definition->initializer =
          make_binding_initializer(initializer ? initializer->bindingTableInitializer() : nullptr,
                                   initializer ? initializer->bindingTableInitializer()->bindingTableExpression() : nullptr,
                                   GQLBindingInitializer::Kind::BindingTable);
      appendClause(definition->children, definition->initializer);
      return Ptr(definition);
    }

    if (auto *value_definition = binding_context->valueVariableDefinition()) {
      auto definition = make_intrusive<GQLBindingVariableDefinition>(GQLBindingVariableDefinition::Kind::Value);
      definition->name = getText(value_definition->bindingVariable());
      auto *initializer = value_definition->optTypedValueInitializer();
      definition->raw_type =
          getTypedRawType(initializer ? initializer->typed() : nullptr, initializer ? initializer->valueType() : nullptr);
      auto *vi = initializer ? initializer->valueInitializer() : nullptr;
      if (!vi) throwUnsupported("binding variable initializer", initializer);
      definition->initializer = makeBindingInitializer(GQLBindingInitializer::Kind::Value, castAny<Ptr>(visit(vi)));
      appendClause(definition->children, definition->initializer);
      return Ptr(definition);
    }

    throwUnsupported("binding variable definition", binding_context);
  };

  auto clause = make_intrusive<GQLSubquery>();

  if (auto *at_schema = procedure_body->atSchemaClause()) {
    clause->at_schema = Ptr(make_intrusive<GQLAtSchemaClause>(makeSchemaReference(at_schema->schemaReference())));
    appendClause(clause->children, clause->at_schema);
  }

  if (auto *bindings = procedure_body->bindingVariableDefinitionBlock()) {
    auto block = make_intrusive<GQLBindingVariableDefinitionBlock>();

    for (auto *binding : bindings->bindingVariableDefinition()) {
      auto binding_definition = make_binding_definition(binding);
      block->definitions.push_back(binding_definition);
      appendClause(block->children, binding_definition);
    }

    clause->bindings = Ptr(block);
    appendClause(clause->children, clause->bindings);
  }

  clause->query = visit_statement(statement_block->statement());
  appendClause(clause->children, clause->query);

  for (auto *next_statement : statement_block->nextStatement()) {
    auto next_clause = make_intrusive<GQLSubqueryNextClause>();

    if (next_statement->yieldClause()) {
      next_clause->yield = makeYieldClause(next_statement->yieldClause());
      appendClause(next_clause->children, next_clause->yield);
    }

    next_clause->query = visit_statement(next_statement->statement());
    appendClause(next_clause->children, next_clause->query);

    clause->next_statements.push_back(next_clause);
    appendClause(clause->children, Ptr(next_clause));
  }

  return Ptr(clause);
}

std::any GQLParseTreeVisitor::visitCompositeQueryStatement(GQLParser::CompositeQueryStatementContext *context) {
  return castAny<Ptr>(visit(context->compositeQueryExpression()));
}

std::any GQLParseTreeVisitor::visitCompositeQueryExpression(GQLParser::CompositeQueryExpressionContext *context) {
  if (!context->compositeQueryExpression()) {
    return castAny<Ptr>(visit(context->compositeQueryPrimary()));
  }

  auto left = castAny<Ptr>(visit(context->compositeQueryExpression()));
  auto right = castAny<Ptr>(visit(context->compositeQueryPrimary()));
  PtrList queries;
  std::vector<CombinedQueryOperator> operators;

  appendCombinedQueryPart(std::move(left), queries, operators);
  operators.push_back(getCombinedQueryOperator(context));
  appendCombinedQueryPart(std::move(right), queries, operators);

  if (queries.size() == 1) return queries.front();

  while (!operators.empty() && operators.size() + 1 > queries.size()) {
    operators.pop_back();
  }

  return Ptr(make_intrusive<GQLCombinedQuery>(std::move(queries), std::move(operators)));
}

std::any GQLParseTreeVisitor::visitCompositeQueryPrimary(GQLParser::CompositeQueryPrimaryContext *context) {
  return castAny<Ptr>(visit(context->linearQueryStatement()));
}

std::any GQLParseTreeVisitor::visitNestedQuerySpecification(GQLParser::NestedQuerySpecificationContext *context) {
  return buildSubquery(context->procedureBody(), context);
}

std::any GQLParseTreeVisitor::visitFocusedNestedQuerySpecification(GQLParser::FocusedNestedQuerySpecificationContext *context) {
  PtrList clauses;
  appendClause(clauses, makeUseGraphClause(context->useGraphClause(), *this));
  appendClause(clauses, castAny<Ptr>(visit(context->nestedQuerySpecification())));
  return makeSingleQuery(std::move(clauses));
}

std::any GQLParseTreeVisitor::visitSelectStatement(GQLParser::SelectStatementContext *context) {
  const bool distinct = context->setQuantifier() && context->setQuantifier()->DISTINCT();
  const bool select_all = context->ASTERISK() != nullptr;

  PtrList items;

  if (!select_all) {
    if (auto *items_context = context->selectItemList()) {
      for (auto *item_context : items_context->selectItem()) {
        auto expression = castAny<Ptr>(visit(item_context->aggregatingValueExpression()));
        String alias;

        if (item_context->selectItemAlias()) alias = getText(item_context->selectItemAlias()->identifier());

        items.push_back(makeAliasedItem(expression, std::move(alias)));
      }
    }
  }

  Ptr source;

  if (auto *select_body = context->selectStatementBody()) {
    if (auto *query_spec = select_body->selectQuerySpecification()) {
      auto *nested_query = query_spec->nestedQuerySpecification();

      if (!nested_query) {
        throwUnsupported("select query specification", query_spec);
      }

      auto source_query = castAny<Ptr>(visit(nested_query));

      if (query_spec->graphExpression())
        source = makeSelectSourceItem(makeGraphExpression(query_spec->graphExpression(), *this), std::move(source_query));
      else
        source = std::move(source_query);
    } else if (auto *match_list = select_body->selectGraphMatchList()) {
      auto source_list = make_intrusive<GQLSelectSourceList>();

      for (auto *match_item : match_list->selectGraphMatch()) {
        auto source_item = makeSelectSourceItem(makeGraphExpression(match_item->graphExpression(), *this),
                                                castAny<Ptr>(visit(match_item->matchStatement())));
        source_list->items.push_back(source_item);
        appendClause(source_list->children, source_item);
      }

      source = Ptr(source_list);
    }
  }

  Ptr where;
  if (context->whereClause()) {
    where = castAny<Ptr>(visit(context->whereClause()));
  }

  Ptr group_by;
  if (context->groupByClause()) {
    group_by = castAny<Ptr>(visit(context->groupByClause()));
  }

  Ptr having;
  if (context->havingClause()) {
    having =
        Ptr(make_intrusive<GQLWhereClause>(GQLWhereClause::Type::Having, castAny<Ptr>(visit(context->havingClause()->searchCondition()))));
  }

  Ptr order_by;
  if (context->orderByClause()) {
    order_by = castAny<Ptr>(visit(context->orderByClause()));
  }

  Ptr offset;
  if (context->offsetClause()) {
    offset = castAny<Ptr>(visit(context->offsetClause()));
  }

  Ptr limit;
  if (context->limitClause()) {
    limit = castAny<Ptr>(visit(context->limitClause()));
  }

  auto clause = make_intrusive<GQLSelectClause>();
  clause->distinct = distinct;
  clause->select_all = select_all;
  clause->items = std::move(items);
  clause->source = std::move(source);
  clause->where = std::move(where);
  clause->group_by = std::move(group_by);
  clause->having = std::move(having);

  for (auto &item : clause->items) {
    appendClause(clause->children, item);
  }

  appendClause(clause->children, clause->source);
  appendClause(clause->children, clause->where);
  appendClause(clause->children, clause->group_by);
  appendClause(clause->children, clause->having);

  PtrList clauses;
  appendClause(clauses, Ptr(clause));

  if (order_by || offset || limit) {
    auto page_clause = make_intrusive<GQLPageClause>();
    page_clause->order_by = std::move(order_by);
    page_clause->offset = std::move(offset);
    page_clause->limit = std::move(limit);
    appendClause(page_clause->children, page_clause->order_by);
    appendClause(page_clause->children, page_clause->offset);
    appendClause(page_clause->children, page_clause->limit);
    appendClause(clauses, Ptr(page_clause));
  }

  return makeSingleQuery(std::move(clauses));
}

std::any GQLParseTreeVisitor::visitLinearQueryStatement(GQLParser::LinearQueryStatementContext *context) {
  if (context->ambientLinearQueryStatement()) {
    return castAny<Ptr>(visit(context->ambientLinearQueryStatement()));
  }

  auto *focused = context->focusedLinearQueryStatement();

  if (!focused) throwUnsupported("focused linear query statement", context);

  if (focused->selectStatement()) return castAny<Ptr>(visit(focused->selectStatement()));

  if (focused->focusedNestedQuerySpecification()) return castAny<Ptr>(visit(focused->focusedNestedQuerySpecification()));

  PtrList clauses;

  for (auto *part : focused->focusedLinearQueryStatementPart()) {
    appendClause(clauses, makeUseGraphClause(part->useGraphClause(), *this));
    appendClauseList(clauses, castAny<PtrList>(visit(part->simpleLinearQueryStatement())));
  }

  if (auto *part = focused->focusedLinearQueryAndPrimitiveResultStatementPart()) {
    appendClause(clauses, makeUseGraphClause(part->useGraphClause(), *this));
    appendClauseList(clauses, castAny<PtrList>(visit(part->simpleLinearQueryStatement())));
    appendQueryResultClause(clauses, castAny<Ptr>(visit(part->primitiveResultStatement())));
  } else if (auto *primitive_result_part = focused->focusedPrimitiveResultStatement()) {
    appendClause(clauses, makeUseGraphClause(primitive_result_part->useGraphClause(), *this));
    appendQueryResultClause(clauses, castAny<Ptr>(visit(primitive_result_part->primitiveResultStatement())));
  }

  if (!clauses.empty()) return makeSingleQuery(std::move(clauses));

  throwUnsupported("focused linear query statement", focused);
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
    appendQueryResultClause(clauses, castAny<Ptr>(visit(context->primitiveResultStatement())));
  }

  return makeSingleQuery(std::move(clauses));
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

std::any GQLParseTreeVisitor::visitCallQueryStatement(GQLParser::CallQueryStatementContext *context) {
  return makeCallClause(context->callProcedureStatement(), *this);
}

std::any GQLParseTreeVisitor::visitPrimitiveQueryStatement(GQLParser::PrimitiveQueryStatementContext *context) {
  if (context->matchStatement()) {
    return castAny<Ptr>(visit(context->matchStatement()));
  }

  if (context->letStatement()) {
    auto clause = make_intrusive<GQLLetClause>();

    for (auto *item : context->letStatement()->letVariableDefinitionList()->letVariableDefinition()) {
      Ptr assignment;

      if (auto *value_definition = item->valueVariableDefinition()) {
        Ptr value;
        String raw_type;

        if (auto *initializer = value_definition->optTypedValueInitializer()) {
          if (initializer->valueType()) raw_type = getText(initializer->valueType());

          if (initializer->valueInitializer()) value = castAny<Ptr>(visit(initializer->valueInitializer()));
        }

        assignment = Ptr(
            make_intrusive<GQLAssignmentItem>(getText(value_definition->bindingVariable()), std::move(value), true, std::move(raw_type)));
      } else {
        assignment = Ptr(make_intrusive<GQLAssignmentItem>(getText(item->bindingVariable()), castAny<Ptr>(visit(item->valueExpression()))));
      }

      clause->items.push_back(assignment);
      appendClause(clause->children, assignment);
    }

    return Ptr(clause);
  }

  if (context->forStatement()) {
    auto clause = make_intrusive<GQLForClause>();
    auto *statement = context->forStatement();
    clause->alias = getText(statement->forItem()->forItemAlias()->bindingVariable());
    clause->source = castAny<Ptr>(visit(statement->forItem()->forItemSource()));
    appendClause(clause->children, clause->source);

    if (auto *ordinality_or_offset = statement->forOrdinalityOrOffset()) {
      clause->with_ordinality = ordinality_or_offset->ORDINALITY();
      clause->with_offset = ordinality_or_offset->OFFSET();
      clause->ordinality_or_offset_alias = getText(ordinality_or_offset->bindingVariable());
    }

    return Ptr(clause);
  }

  if (context->filterStatement()) {
    return castAny<Ptr>(visit(context->filterStatement()));
  }

  if (context->orderByAndPageStatement()) {
    auto tail = castAny<ResultTail>(visit(context->orderByAndPageStatement()));
    return makeOrderByAndPageClause(std::move(tail));
  }

  throwUnsupported("primitive query statement", context);
}

}  // namespace OPENGQL

}  // namespace DB
