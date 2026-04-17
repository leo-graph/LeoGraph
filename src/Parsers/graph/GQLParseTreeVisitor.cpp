#include <Common/Exception.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>

// `antlr4-common.h` undefines `EOF`, but `boost::multiprecision` still relies on it.
#if !defined(EOF)
#  define EOF (-1)
#endif

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
  GraphMatchMode match_mode = GraphMatchMode::None;
  Ptr keep_clause;
  PtrList path_patterns;
  Ptr where;
  PtrList yield_items;
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

CombinedQueryOperator getCombinedQueryOperator(GQLParser::CompositeQueryExpressionContext *context) {
  if (auto *conjunction = context->queryConjunction()) {
    if (auto *set_operator = conjunction->setOperator()) {
      const bool is_all = set_operator->setQuantifier() && set_operator->setQuantifier()->ALL();

      if (set_operator->UNION()) return is_all ? CombinedQueryOperator::UnionAll : CombinedQueryOperator::UnionDistinct;

      if (set_operator->EXCEPT()) return is_all ? CombinedQueryOperator::ExceptAll : CombinedQueryOperator::ExceptDistinct;

      if (set_operator->INTERSECT()) return is_all ? CombinedQueryOperator::IntersectAll : CombinedQueryOperator::IntersectDistinct;
    }

    if (conjunction->OTHERWISE()) return CombinedQueryOperator::Otherwise;
  }

  return CombinedQueryOperator::UnionDistinct;
}

void appendCombinedQueryPart(Ptr query, PtrList &queries, std::vector<CombinedQueryOperator> &operators) {
  if (!query) return;

  if (const auto *combined_query = query->as<GQLCombinedQuery>()) {
    for (const auto &child_query : combined_query->queries) queries.push_back(child_query);

    for (const auto operation : combined_query->operators) operators.push_back(operation);

    return;
  }

  queries.push_back(std::move(query));
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

Ptr makeRawTextClause(const String &text) { return GQLExpr::rawText(text); }

void appendClause(PtrList &clauses, Ptr clause) {
  if (clause) clauses.push_back(std::move(clause));
}

String getElementVariableName(GQLParser::ElementVariableReferenceContext *context) {
  auto *binding_reference = context ? context->bindingVariableReference() : nullptr;
  auto *binding_variable = binding_reference ? binding_reference->bindingVariable() : nullptr;
  return getText(binding_variable);
}

String getTypedRawType(GQLParser::TypedContext *typed_context, antlr4::ParserRuleContext *type_context) {
  String result;

  if (typed_context) {
    result += getText(typed_context);
  }

  if (type_context) {
    if (!result.empty()) {
      result += " ";
    }

    result += getText(type_context);
  }

  return result;
}

std::vector<String> getDirectoryParts(GQLParser::SimpleDirectoryPathContext *context) {
  std::vector<String> parts;

  if (!context) {
    return parts;
  }

  for (auto *directory_name : context->directoryName()) {
    parts.push_back(getText(directory_name));
  }

  return parts;
}

Ptr makeSchemaReference(GQLParser::SchemaReferenceContext *context) {
  if (!context) {
    return {};
  }

  if (auto *absolute = context->absoluteCatalogSchemaReference()) {
    if (!absolute->schemaName()) {
      return Ptr(make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::AbsoluteRoot));
    }

    auto reference = make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::AbsolutePath, getText(absolute->schemaName()));

    if (auto *absolute_directory = absolute->absoluteDirectoryPath()) {
      reference->directory_parts = getDirectoryParts(absolute_directory->simpleDirectoryPath());
    }

    return Ptr(reference);
  }

  if (auto *relative = context->relativeCatalogSchemaReference()) {
    if (auto *predefined = relative->predefinedSchemaReference()) {
      return Ptr(make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::Predefined, getText(predefined)));
    }

    auto reference = make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::RelativePath, getText(relative->schemaName()));

    if (auto *relative_directory = relative->relativeDirectoryPath()) {
      reference->parent_levels = relative_directory->DOUBLE_PERIOD().size();
      reference->directory_parts = getDirectoryParts(relative_directory->simpleDirectoryPath());
    }

    return Ptr(reference);
  }

  if (auto *parameter = context->referenceParameterSpecification()) {
    return Ptr(make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::Parameter, getText(parameter)));
  }

  throwUnsupported("schema reference", context);
}

Ptr makeBindingInitializer(GQLBindingInitializer::Kind kind, Ptr value) {
  return Ptr(make_intrusive<GQLBindingInitializer>(kind, std::move(value)));
}

Ptr makeGraphExpression(GQLParser::GraphExpressionContext *context, GQLParseTreeVisitor &) {
  if (!context) {
    return {};
  }

  if (context->graphReference()) {
    return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::GraphReference, getText(context->graphReference())));
  }

  if (context->currentGraph()) {
    return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::CurrentGraph, getText(context->currentGraph())));
  }

  if (context->objectNameOrBindingVariable()) {
    return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::ObjectNameOrBindingVariable,
                                                  getText(context->objectNameOrBindingVariable())));
  }

  if (context->objectExpressionPrimary()) {
    return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::ObjectExpression, String{},
                                                  makeRawTextClause(getText(context->objectExpressionPrimary()))));
  }

  throwUnsupported("graph expression", context);
}

Ptr makeBindingTableExpression(GQLParser::BindingTableExpressionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) {
    return {};
  }

  if (context->nestedBindingTableQuerySpecification()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(
        GQLBindingTableExpression::Kind::NestedQuery, String{},
        castAny<Ptr>(visitor.visit(context->nestedBindingTableQuerySpecification()->nestedQuerySpecification()))));
  }

  if (context->bindingTableReference()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::BindingTableReference,
                                                         getText(context->bindingTableReference())));
  }

  if (context->objectNameOrBindingVariable()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::ObjectNameOrBindingVariable,
                                                         getText(context->objectNameOrBindingVariable())));
  }

  if (context->objectExpressionPrimary()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::ObjectExpression, String{},
                                                         makeRawTextClause(getText(context->objectExpressionPrimary()))));
  }

  throwUnsupported("binding table expression", context);
}

PtrList makeYieldItems(GQLParser::YieldItemListContext *context) {
  PtrList items;

  if (!context) {
    return items;
  }

  for (auto *item : context->yieldItem()) {
    auto expression = GQLExpr::identifier(getText(item->yieldItemName()->fieldName()));

    if (item->yieldItemAlias()) {
      if (auto *with_alias = dynamic_cast<ASTWithAlias *>(expression.get()))
        with_alias->setAlias(getText(item->yieldItemAlias()->bindingVariable()));
      else
        throwUnsupported("yield item alias on non-aliasable expression", item);
    }

    items.push_back(expression);
  }

  return items;
}

Ptr makeYieldClause(GQLParser::YieldClauseContext *context) {
  auto clause = make_intrusive<GQLYieldClause>();
  clause->items = makeYieldItems(context ? context->yieldItemList() : nullptr);

  for (auto &item : clause->items) {
    appendClause(clause->children, item);
  }

  return Ptr(clause);
}

Ptr makeUseGraphClause(GQLParser::UseGraphClauseContext *context, GQLParseTreeVisitor &visitor) {
  return Ptr(make_intrusive<GQLUseClause>(makeGraphExpression(context ? context->graphExpression() : nullptr, visitor)));
}

Ptr makeSingleQuery(PtrList clauses) { return Ptr(make_intrusive<GQLSingleQuery>(std::move(clauses))); }

Ptr makeSelectSourceItem(Ptr graph_reference, Ptr source) {
  auto item = make_intrusive<GQLSelectSourceItem>();
  item->graph_reference = std::move(graph_reference);
  item->source = std::move(source);
  appendClause(item->children, item->graph_reference);
  appendClause(item->children, item->source);
  return Ptr(item);
}

void appendClauseList(PtrList &clauses, PtrList &&extra_clauses) {
  for (auto &clause : extra_clauses) appendClause(clauses, std::move(clause));
}

void appendQueryResultClause(PtrList &clauses, Ptr clause) {
  if (!clause) return;

  if (const auto *single_query = clause->as<GQLSingleQuery>()) {
    for (const auto &nested_clause : single_query->clauses) clauses.push_back(nested_clause);

    return;
  }

  clauses.push_back(std::move(clause));
}

PathMode getPathMode(GQLParser::PathModeContext *context) {
  if (!context) {
    return PathMode::None;
  }

  if (context->WALK()) {
    return PathMode::Walk;
  }

  if (context->TRAIL()) {
    return PathMode::Trail;
  }

  if (context->SIMPLE()) {
    return PathMode::Simple;
  }

  if (context->ACYCLIC()) {
    return PathMode::Acyclic;
  }

  return PathMode::None;
}

Ptr makePathPatternPrefix(GQLParser::PathModePrefixContext *context) {
  if (!context) {
    return {};
  }

  auto prefix = make_intrusive<GQLPathPatternPrefix>();
  prefix->path_mode = getPathMode(context->pathMode());
  prefix->has_path_keyword = context->pathOrPaths() != nullptr;
  prefix->use_paths_keyword = context->pathOrPaths() && context->pathOrPaths()->PATHS();
  return Ptr(prefix);
}

Ptr makePathPatternPrefix(GQLParser::PathPatternPrefixContext *context) {
  if (!context) {
    return {};
  }

  if (context->pathModePrefix()) {
    return makePathPatternPrefix(context->pathModePrefix());
  }

  auto *search = context->pathSearchPrefix();
  if (!search) {
    return {};
  }

  auto prefix = make_intrusive<GQLPathPatternPrefix>();

  if (auto *all = search->allPathSearch()) {
    prefix->search_kind = PathSearchKind::All;
    prefix->path_mode = getPathMode(all->pathMode());
    prefix->has_path_keyword = all->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = all->pathOrPaths() && all->pathOrPaths()->PATHS();
    return Ptr(prefix);
  }

  if (auto *any = search->anyPathSearch()) {
    prefix->search_kind = PathSearchKind::Any;
    prefix->count = any->numberOfPaths() ? getText(any->numberOfPaths()->nonNegativeIntegerSpecification()) : String{};
    prefix->path_mode = getPathMode(any->pathMode());
    prefix->has_path_keyword = any->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = any->pathOrPaths() && any->pathOrPaths()->PATHS();
    return Ptr(prefix);
  }

  auto *shortest = search->shortestPathSearch();
  if (!shortest) {
    return Ptr(prefix);
  }

  if (auto *all_shortest = shortest->allShortestPathSearch()) {
    prefix->search_kind = PathSearchKind::AllShortest;
    prefix->path_mode = getPathMode(all_shortest->pathMode());
    prefix->has_path_keyword = all_shortest->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = all_shortest->pathOrPaths() && all_shortest->pathOrPaths()->PATHS();
    return Ptr(prefix);
  }

  if (auto *any_shortest = shortest->anyShortestPathSearch()) {
    prefix->search_kind = PathSearchKind::AnyShortest;
    prefix->path_mode = getPathMode(any_shortest->pathMode());
    prefix->has_path_keyword = any_shortest->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = any_shortest->pathOrPaths() && any_shortest->pathOrPaths()->PATHS();
    return Ptr(prefix);
  }

  if (auto *counted_shortest = shortest->countedShortestPathSearch()) {
    prefix->search_kind = PathSearchKind::CountedShortest;
    prefix->count = getText(counted_shortest->numberOfPaths()->nonNegativeIntegerSpecification());
    prefix->path_mode = getPathMode(counted_shortest->pathMode());
    prefix->has_path_keyword = counted_shortest->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = counted_shortest->pathOrPaths() && counted_shortest->pathOrPaths()->PATHS();
    return Ptr(prefix);
  }

  if (auto *counted_group = shortest->countedShortestGroupSearch()) {
    prefix->search_kind = PathSearchKind::CountedShortestGroup;
    prefix->count =
        counted_group->numberOfGroups() ? getText(counted_group->numberOfGroups()->nonNegativeIntegerSpecification()) : String{};
    prefix->path_mode = getPathMode(counted_group->pathMode());
    prefix->has_path_keyword = counted_group->pathOrPaths() != nullptr;
    prefix->use_paths_keyword = counted_group->pathOrPaths() && counted_group->pathOrPaths()->PATHS();
    prefix->use_groups_keyword = counted_group->GROUPS() != nullptr;
    return Ptr(prefix);
  }

  return Ptr(prefix);
}

GraphMatchMode getGraphMatchMode(GQLParser::MatchModeContext *context) {
  if (!context) {
    return GraphMatchMode::None;
  }

  if (auto *repeatable = context->repeatableElementsMatchMode()) {
    return repeatable->elementBindingsOrElements() && repeatable->elementBindingsOrElements()->BINDINGS()
               ? GraphMatchMode::RepeatableElementBindings
               : GraphMatchMode::RepeatableElements;
  }

  if (auto *different = context->differentEdgesMatchMode()) {
    return different->edgeBindingsOrEdges() && different->edgeBindingsOrEdges()->BINDINGS() ? GraphMatchMode::DifferentEdgeBindings
                                                                                            : GraphMatchMode::DifferentEdges;
  }

  return GraphMatchMode::None;
}

PtrList makeGraphPatternYieldItems(GQLParser::GraphPatternYieldItemListContext *context) {
  PtrList items;

  if (!context) {
    return items;
  }

  for (auto *item : context->graphPatternYieldItem()) {
    auto *binding_reference = item->bindingVariableReference();
    auto *binding_variable = binding_reference ? binding_reference->bindingVariable() : nullptr;
    items.push_back(GQLExpr::identifier(getText(binding_variable)));
  }

  return items;
}

Ptr makeMatchClause(PatternBindingTable &&binding_table, bool optional = false) {
  auto clause = make_intrusive<GQLMatchClause>(optional);
  clause->match_mode = binding_table.match_mode;
  clause->keep_clause = std::move(binding_table.keep_clause);
  clause->path_patterns = std::move(binding_table.path_patterns);
  clause->where = std::move(binding_table.where);
  clause->yield_items = std::move(binding_table.yield_items);

  for (const auto &path : clause->path_patterns) {
    if (path) {
      clause->children.push_back(path);
    }
  }

  if (clause->keep_clause) {
    clause->children.push_back(clause->keep_clause);
  }

  if (clause->where) {
    clause->children.push_back(clause->where);
  }

  for (const auto &item : clause->yield_items) {
    if (item) {
      clause->children.push_back(item);
    }
  }

  return Ptr(clause);
}

Ptr makeGraphPatternBlock(PatternBindingTable &&binding_table, bool parenthesized) {
  auto block = make_intrusive<GQLGraphPatternBlock>();
  block->parenthesized = parenthesized;
  block->match_mode = binding_table.match_mode;
  block->keep_clause = std::move(binding_table.keep_clause);
  block->path_patterns = std::move(binding_table.path_patterns);
  block->where = std::move(binding_table.where);

  for (const auto &path : block->path_patterns) {
    appendClause(block->children, path);
  }

  appendClause(block->children, block->keep_clause);
  appendClause(block->children, block->where);

  return Ptr(block);
}

void populateMatchStatementBlock(GQLMatchStatementBlock &block, GQLParseTreeVisitor &visitor,
                                 GQLParser::MatchStatementBlockContext *context) {
  if (!context) {
    return;
  }

  for (auto *match_statement : context->matchStatement()) {
    auto match_clause = castAny<Ptr>(visitor.visit(match_statement));
    block.matches.push_back(match_clause);
    appendClause(block.children, match_clause);
  }
}

Ptr makeOrderByAndPageClause(ResultTail &&tail) {
  auto clause = make_intrusive<GQLPageClause>();
  clause->order_by = std::move(tail.order_by);
  clause->offset = std::move(tail.offset);
  clause->limit = std::move(tail.limit);

  appendClause(clause->children, clause->order_by);
  appendClause(clause->children, clause->offset);
  appendClause(clause->children, clause->limit);

  return Ptr(clause);
}

Ptr makeListConstructor(GQLParser::ListValueConstructorByEnumerationContext *context, GQLParseTreeVisitor &visitor) {
  auto list = make_intrusive<GQLListConstructor>();

  auto *elements = context ? context->listElementList() : nullptr;
  if (!elements) {
    return Ptr(list);
  }

  for (auto *element : elements->listElement()) {
    auto item = castAny<Ptr>(visitor.visit(element->valueExpression()));
    list->items.push_back(item);
    appendClause(list->children, item);
  }

  return Ptr(list);
}

Ptr makeRecordConstructor(GQLParser::RecordConstructorContext *context, GQLParseTreeVisitor &visitor) {
  auto record = make_intrusive<GQLRecordConstructor>();
  record->explicit_record_keyword = context && context->RECORD() != nullptr;

  auto *field_list = context && context->fieldsSpecification() ? context->fieldsSpecification()->fieldList() : nullptr;
  if (!field_list) {
    return Ptr(record);
  }

  for (auto *field : field_list->field()) {
    auto item = Ptr(
        make_intrusive<GQLPropertyItem>(getText(field->fieldName()->identifier()), castAny<Ptr>(visitor.visit(field->valueExpression()))));
    record->fields.push_back(item);
    appendClause(record->children, item);
  }

  return Ptr(record);
}

Ptr makeAggregateFunction(GQLParser::AggregateFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) {
    return {};
  }

  if (context->COUNT() && context->ASTERISK()) {
    PtrList arguments;
    arguments.push_back(GQLExpr::literal("*"));
    return GQLExpr::functionCall("COUNT", std::move(arguments));
  }

  if (auto *general = context->generalSetFunction()) {
    Ptr argument = castAny<Ptr>(visitor.visit(general->valueExpression()));
    if (general->setQuantifier()) {
      String prefix = general->setQuantifier()->DISTINCT() ? "DISTINCT " : "ALL ";
      argument = GQLExpr::rawText(prefix + detail::formatNodeToString(*argument));
    }

    PtrList arguments;
    arguments.push_back(argument);
    return GQLExpr::functionCall(getText(general->generalSetFunctionType()), std::move(arguments));
  }

  return GQLExpr::rawText(getText(context));
}

Ptr makeValueFunction(GQLParser::ValueFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) {
    return {};
  }

  auto *numeric = context->numericValueFunction();
  if (!numeric) {
    return GQLExpr::rawText(getText(context));
  }

  if (auto *length = numeric->lengthExpression()) {
    if (auto *char_length = length->charLengthExpression()) {
      PtrList arguments;
      arguments.push_back(castAny<Ptr>(visitor.visit(char_length->characterStringValueExpression()->valueExpression())));
      return GQLExpr::functionCall(char_length->CHAR_LENGTH() ? "CHAR_LENGTH" : "CHARACTER_LENGTH", std::move(arguments));
    }

    if (auto *byte_length = length->byteLengthExpression()) {
      PtrList arguments;
      arguments.push_back(castAny<Ptr>(visitor.visit(byte_length->byteStringValueExpression()->valueExpression())));
      return GQLExpr::functionCall(byte_length->BYTE_LENGTH() ? "BYTE_LENGTH" : "OCTET_LENGTH", std::move(arguments));
    }

    if (auto *path_length = length->pathLengthExpression()) {
      PtrList arguments;
      arguments.push_back(castAny<Ptr>(visitor.visit(path_length->pathValueExpression()->valueExpression())));
      return GQLExpr::functionCall("PATH_LENGTH", std::move(arguments));
    }
  }

  if (auto *cardinality = numeric->cardinalityExpression()) {
    PtrList arguments;
    if (cardinality->cardinalityExpressionArgument()) {
      arguments.push_back(castAny<Ptr>(visitor.visit(cardinality->cardinalityExpressionArgument()->valueExpression())));
      return GQLExpr::functionCall("CARDINALITY", std::move(arguments));
    }

    if (cardinality->listValueExpression()) {
      arguments.push_back(castAny<Ptr>(visitor.visit(cardinality->listValueExpression()->valueExpression())));
      return GQLExpr::functionCall("SIZE", std::move(arguments));
    }
  }

  if (auto *absolute = numeric->absoluteValueExpression()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(absolute->valueExpression())));
    return GQLExpr::functionCall("ABS", std::move(arguments));
  }

  return GQLExpr::rawText(getText(context));
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

Ptr GQLParseTreeVisitor::buildSubquery(GQLParser::ProcedureBodyContext *procedure_body, antlr4::ParserRuleContext *context) {
  auto *statement_block = procedure_body ? procedure_body->statementBlock() : nullptr;

  if (!procedure_body || !statement_block || !statement_block->statement()) {
    throwUnsupported("nested query specification body", context);
  }

  auto visit_statement = [this](GQLParser::StatementContext *statement) -> Ptr {
    if (!statement) return {};

    if (statement->compositeQueryStatement()) return castAny<Ptr>(visit(statement->compositeQueryStatement()));

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
      definition->initializer = make_binding_initializer(initializer ? initializer->valueInitializer() : nullptr,
                                                         initializer ? initializer->valueInitializer()->valueExpression() : nullptr,
                                                         GQLBindingInitializer::Kind::Value);
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

        if (item_context->selectItemAlias()) {
          if (auto *with_alias = dynamic_cast<ASTWithAlias *>(expression.get()))
            with_alias->setAlias(getText(item_context->selectItemAlias()->identifier()));
          else
            throwUnsupported("select item alias on non-aliasable expression", item_context);
        }

        items.push_back(expression);
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
  auto *statement = context->callProcedureStatement();
  auto *procedure_call = statement->procedureCall();

  if (auto *named = procedure_call->namedProcedureCall()) {
    auto clause = make_intrusive<GQLCallNamedClause>();
    clause->optional = statement->OPTIONAL() != nullptr;
    clause->procedure = GQLExpr::identifier(getText(named->procedureReference()));

    if (auto *argument_list = named->procedureArgumentList()) {
      for (auto *argument : argument_list->procedureArgument())
        clause->arguments.push_back(castAny<Ptr>(visit(argument->valueExpression())));
    }

    if (auto *yield_clause = named->yieldClause()) {
      clause->yield = makeYieldClause(yield_clause);
    }

    appendClause(clause->children, clause->procedure);
    for (auto &argument : clause->arguments) appendClause(clause->children, argument);
    appendClause(clause->children, clause->yield);
    return Ptr(clause);
  }

  if (auto *inline_call = procedure_call->inlineProcedureCall()) {
    if (inline_call->variableScopeClause()) {
      throwUnsupported("inline call variable scope", inline_call->variableScopeClause());
    }

    auto clause = make_intrusive<GQLCallInlineClause>();
    clause->optional = statement->OPTIONAL() != nullptr;
    clause->subquery = buildSubquery(inline_call->nestedProcedureSpecification()->procedureSpecification()->procedureBody(), inline_call);
    appendClause(clause->children, clause->subquery);
    return Ptr(clause);
  }

  throwUnsupported("procedure call", procedure_call);
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

          if (initializer->valueInitializer()) value = castAny<Ptr>(visit(initializer->valueInitializer()->valueExpression()));
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
    clause->source = castAny<Ptr>(visit(statement->forItem()->forItemSource()->valueExpression()));
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

std::any GQLParseTreeVisitor::visitMatchStatement(GQLParser::MatchStatementContext *context) {
  if (context->simpleMatchStatement()) {
    return castAny<Ptr>(visit(context->simpleMatchStatement()));
  }

  return castAny<Ptr>(visit(context->optionalMatchStatement()));
}

std::any GQLParseTreeVisitor::visitSimpleMatchStatement(GQLParser::SimpleMatchStatementContext *context) {
  auto binding_table = castAny<PatternBindingTable>(visit(context->graphPatternBindingTable()));
  return makeMatchClause(std::move(binding_table));
}

std::any GQLParseTreeVisitor::visitOptionalMatchStatement(GQLParser::OptionalMatchStatementContext *context) {
  auto *operand = context->optionalOperand();
  if (operand->simpleMatchStatement()) {
    auto clause = castAny<Ptr>(visit(operand->simpleMatchStatement()));
    if (auto *match_clause = clause->as<GQLMatchClause>()) {
      match_clause->optional = true;
      return clause;
    }

    throwUnsupported("optional match clause", context);
  }

  if (auto *block_context = operand->matchStatementBlock()) {
    auto clause = make_intrusive<GQLMatchClause>(true);
    auto block = make_intrusive<GQLMatchStatementBlock>();
    block->parenthesized = operand->LEFT_PAREN() != nullptr;
    populateMatchStatementBlock(*block, *this, block_context);
    clause->optional_operand_block = Ptr(block);
    appendClause(clause->children, clause->optional_operand_block);
    return Ptr(clause);
  }

  throwUnsupported("complex optional match operand", context);
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
  auto result = castAny<PatternBindingTable>(visit(context->graphPattern()));
  if (context->graphPatternYieldClause()) {
    result.yield_items = makeGraphPatternYieldItems(context->graphPatternYieldClause()->graphPatternYieldItemList());
  }
  return result;
}

std::any GQLParseTreeVisitor::visitGraphPattern(GQLParser::GraphPatternContext *context) {
  PatternBindingTable result;
  result.match_mode = getGraphMatchMode(context->matchMode());
  result.keep_clause = context->keepClause() ? makePathPatternPrefix(context->keepClause()->pathPatternPrefix()) : Ptr{};
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
  auto elements = castAny<PtrList>(visit(context->pathPatternExpression()));
  auto pattern = make_intrusive<GQLPathPattern>(std::move(elements));

  if (context->pathVariableDeclaration()) {
    pattern->variable = GQLExpr::identifier(getText(context->pathVariableDeclaration()->pathVariable()->bindingVariable()));
    pattern->children.insert(pattern->children.begin(), pattern->variable);
  }

  if (context->pathPatternPrefix()) {
    pattern->prefix = makePathPatternPrefix(context->pathPatternPrefix());
    if (pattern->prefix) {
      auto insert_pos = pattern->variable ? pattern->children.begin() + 1 : pattern->children.begin();
      pattern->children.insert(insert_pos, pattern->prefix);
    }
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

  if (auto *path = node->as<GQLParenthesizedPathPattern>()) {
    path->quantifier = std::move(quantifier);
    if (path->quantifier) {
      path->children.push_back(path->quantifier);
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

  if (auto *path = node->as<GQLParenthesizedPathPattern>()) {
    path->quantifier = makeQuestionQuantifier();
    path->children.push_back(path->quantifier);
    return node;
  }

  throwUnsupported("question-mark non-edge path primary", context);
}

std::any GQLParseTreeVisitor::visitPpElementPattern(GQLParser::PpElementPatternContext *context) {
  return castAny<Ptr>(visit(context->elementPattern()));
}

std::any GQLParseTreeVisitor::visitPpParenthesizedPathPatternExpression(GQLParser::PpParenthesizedPathPatternExpressionContext *context) {
  auto *pattern_context = context->parenthesizedPathPatternExpression();
  auto pattern = make_intrusive<GQLParenthesizedPathPattern>();

  if (pattern_context->subpathVariableDeclaration()) {
    pattern->subpath_variable = GQLExpr::identifier(getText(pattern_context->subpathVariableDeclaration()->subpathVariable()));
    appendClause(pattern->children, pattern->subpath_variable);
  }

  if (pattern_context->pathModePrefix()) {
    pattern->prefix = makePathPatternPrefix(pattern_context->pathModePrefix());
    appendClause(pattern->children, pattern->prefix);
  }

  pattern->elements = castAny<PtrList>(visit(pattern_context->pathPatternExpression()));
  appendClauseList(pattern->children, PtrList(pattern->elements.begin(), pattern->elements.end()));

  if (pattern_context->parenthesizedPathPatternWhereClause()) {
    pattern->where =
        Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(pattern_context->parenthesizedPathPatternWhereClause()->searchCondition()))));
    appendClause(pattern->children, pattern->where);
  }

  return Ptr(pattern);
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

  if (!context->orderByAndPageStatement()) return clause;

  auto tail = castAny<ResultTail>(visit(context->orderByAndPageStatement()));
  auto page_clause = make_intrusive<GQLPageClause>();
  page_clause->order_by = std::move(tail.order_by);
  page_clause->offset = std::move(tail.offset);
  page_clause->limit = std::move(tail.limit);
  appendClause(page_clause->children, page_clause->order_by);
  appendClause(page_clause->children, page_clause->offset);
  appendClause(page_clause->children, page_clause->limit);

  PtrList clauses;
  appendClause(clauses, std::move(clause));
  appendClause(clauses, Ptr(page_clause));
  return makeSingleQuery(std::move(clauses));
}

std::any GQLParseTreeVisitor::visitReturnStatement(GQLParser::ReturnStatementContext *context) {
  return castAny<Ptr>(visit(context->returnStatementBody()));
}

std::any GQLParseTreeVisitor::visitReturnStatementBody(GQLParser::ReturnStatementBodyContext *context) {
  auto clause = make_intrusive<GQLReturnClause>();

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

  if (context->groupByClause()) {
    clause->group_by = castAny<Ptr>(visit(context->groupByClause()));

    if (clause->group_by) clause->children.push_back(clause->group_by);
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

std::any GQLParseTreeVisitor::visitGroupByClause(GQLParser::GroupByClauseContext *context) {
  auto clause = make_intrusive<GQLGroupByClause>();

  if (auto *grouping_list = context->groupingElementList()) {
    clause->empty_grouping_set = grouping_list->emptyGroupingSet() != nullptr;

    for (auto *element : grouping_list->groupingElement()) {
      auto *binding_reference = element->bindingVariableReference();
      auto *binding_variable = binding_reference ? binding_reference->bindingVariable() : nullptr;
      auto item = GQLExpr::identifier(getText(binding_variable));
      clause->items.push_back(item);
      appendClause(clause->children, item);
    }
  }

  return Ptr(clause);
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

std::any GQLParseTreeVisitor::visitIsNotExprAlt(GQLParser::IsNotExprAltContext *context) {
  const String op = context->NOT() ? "IS NOT" : "IS";
  return GQLExpr::binaryOp(op, castAny<Ptr>(visit(context->valueExpression())), GQLExpr::literal(getText(context->truthValue())));
}

std::any GQLParseTreeVisitor::visitNotExprAlt(GQLParser::NotExprAltContext *context) {
  return GQLExpr::unaryOp("NOT ", castAny<Ptr>(visit(context->valueExpression())));
}

std::any GQLParseTreeVisitor::visitValueFunctionExprAlt(GQLParser::ValueFunctionExprAltContext *context) {
  return makeValueFunction(context->valueFunction(), *this);
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

std::any GQLParseTreeVisitor::visitPredicateExprAlt(GQLParser::PredicateExprAltContext *context) {
  auto *predicate = context->predicate();

  if (auto *null_predicate = predicate->nullPredicate()) {
    const String op = null_predicate->nullPredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, castAny<Ptr>(visit(null_predicate->valueExpressionPrimary())), GQLExpr::literal("NULL"));
  }

  if (auto *value_type_predicate = predicate->valueTypePredicate()) {
    const String op = value_type_predicate->valueTypePredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, castAny<Ptr>(visit(value_type_predicate->valueExpressionPrimary())),
                             GQLExpr::literal(getText(value_type_predicate->valueTypePredicatePart2()->valueType())));
  }

  if (auto *directed_predicate = predicate->directedPredicate()) {
    const String op = directed_predicate->directedPredicatePart2()->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, GQLExpr::identifier(getElementVariableName(directed_predicate->elementVariableReference())),
                             GQLExpr::literal("DIRECTED"));
  }

  if (auto *labeled_predicate = predicate->labeledPredicate()) {
    auto *label_part = labeled_predicate->labeledPredicatePart2()->isLabeledOrColon();
    String op = ":";

    if (label_part->IS()) op = label_part->NOT() ? "IS NOT LABELED" : "IS LABELED";

    return GQLExpr::binaryOp(op, GQLExpr::identifier(getElementVariableName(labeled_predicate->elementVariableReference())),
                             castAny<Ptr>(visit(labeled_predicate->labeledPredicatePart2()->labelExpression())));
  }

  if (auto *source_destination_predicate = predicate->sourceDestinationPredicate()) {
    auto left = GQLExpr::identifier(getElementVariableName(source_destination_predicate->nodeReference()->elementVariableReference()));

    if (auto *source = source_destination_predicate->sourcePredicatePart2()) {
      const String op = source->NOT() ? "IS NOT SOURCE OF" : "IS SOURCE OF";
      auto right = GQLExpr::identifier(getElementVariableName(source->edgeReference()->elementVariableReference()));
      return GQLExpr::binaryOp(op, left, right);
    }

    auto *destination = source_destination_predicate->destinationPredicatePart2();
    const String op = destination->NOT() ? "IS NOT DESTINATION OF" : "IS DESTINATION OF";
    auto right = GQLExpr::identifier(getElementVariableName(destination->edgeReference()->elementVariableReference()));
    return GQLExpr::binaryOp(op, left, right);
  }

  if (auto *all_different_predicate = predicate->all_differentPredicate()) {
    PtrList arguments;

    for (auto *reference : all_different_predicate->elementVariableReference())
      arguments.push_back(GQLExpr::identifier(getElementVariableName(reference)));

    return GQLExpr::functionCall("ALL_DIFFERENT", std::move(arguments));
  }

  if (auto *same_predicate = predicate->samePredicate()) {
    PtrList arguments;

    for (auto *reference : same_predicate->elementVariableReference())
      arguments.push_back(GQLExpr::identifier(getElementVariableName(reference)));

    return GQLExpr::functionCall("SAME", std::move(arguments));
  }

  if (auto *property_exists_predicate = predicate->property_existsPredicate()) {
    PtrList arguments;
    arguments.push_back(GQLExpr::identifier(getElementVariableName(property_exists_predicate->elementVariableReference())));
    arguments.push_back(GQLExpr::literal(getText(property_exists_predicate->propertyName()->identifier())));
    return GQLExpr::functionCall("PROPERTY_EXISTS", std::move(arguments));
  }

  if (auto *exists_predicate = predicate->existsPredicate()) {
    Ptr operand;

    if (exists_predicate->graphPattern()) {
      auto binding_table = castAny<PatternBindingTable>(visit(exists_predicate->graphPattern()));
      operand = makeGraphPatternBlock(std::move(binding_table), exists_predicate->LEFT_PAREN() != nullptr);
    } else if (exists_predicate->matchStatementBlock()) {
      auto block = make_intrusive<GQLMatchStatementBlock>();
      block->parenthesized = exists_predicate->LEFT_PAREN() != nullptr;
      populateMatchStatementBlock(*block, *this, exists_predicate->matchStatementBlock());
      operand = Ptr(block);
    } else if (exists_predicate->nestedQuerySpecification()) {
      operand = castAny<Ptr>(visit(exists_predicate->nestedQuerySpecification()));
    }

    return GQLExpr::unaryOp("EXISTS ", std::move(operand));
  }

  return makeRawTextExpr(context);
}

std::any GQLParseTreeVisitor::visitValueExpressionPrimary(GQLParser::ValueExpressionPrimaryContext *context) {
  if (context->bindingVariableReference()) {
    return GQLExpr::identifier(getText(context->bindingVariableReference()->bindingVariable()));
  }

  if (context->aggregateFunction()) {
    return makeAggregateFunction(context->aggregateFunction(), *this);
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
  if (auto *general_literal = context->generalLiteral()) {
    if (auto *list_literal = general_literal->listLiteral()) {
      return makeListConstructor(list_literal->listValueConstructorByEnumeration(), *this);
    }

    if (auto *record_literal = general_literal->recordLiteral()) {
      return makeRecordConstructor(record_literal->recordConstructor(), *this);
    }
  }

  return GQLExpr::literal(getText(context));
}

}  // namespace OPENGQL

}  // namespace DB
