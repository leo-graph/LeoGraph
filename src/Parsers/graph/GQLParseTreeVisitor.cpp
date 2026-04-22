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

Ptr makeCountSpec(GQLParser::NonNegativeIntegerSpecificationContext *context) {
  if (!context) {
    return {};
  }

  if (auto *unsigned_integer = context->unsignedInteger()) {
    return Ptr(make_intrusive<GQLCountSpec>(CountSpecKind::Integer, getText(unsigned_integer)));
  }

  if (auto *dynamic_parameter = context->dynamicParameterSpecification()) {
    return Ptr(make_intrusive<GQLCountSpec>(CountSpecKind::DynamicParameter, getText(dynamic_parameter)));
  }

  return {};
}

Ptr makeCallVariableScopeClause(GQLParser::VariableScopeClauseContext *context) {
  if (!context) return {};

  auto clause = make_intrusive<GQLCallVariableScopeClause>();
  auto *reference_list = context->bindingVariableReferenceList();

  if (!reference_list) return Ptr(clause);

  for (auto *reference : reference_list->bindingVariableReference()) {
    auto *binding_variable = reference ? reference->bindingVariable() : nullptr;
    auto variable = GQLExpr::identifier(getText(binding_variable));
    clause->variables.push_back(variable);
    if (variable) clause->children.push_back(variable);
  }

  return Ptr(clause);
}

Ptr makeRawTextClause(const String &text) { return GQLExpr::rawText(text); }

void appendClause(PtrList &clauses, Ptr clause) {
  if (clause) clauses.push_back(std::move(clause));
}

void appendFlattenedSimplifiedOperands(PtrList &operands, GQLSimplifiedPathExpr::Kind kind, const Ptr &operand) {
  if (!operand) {
    return;
  }

  if (const auto *expression = operand->as<GQLSimplifiedPathExpr>();
      expression && expression->kind == kind && !expression->operands.empty()) {
    for (const auto &child : expression->operands) {
      operands.push_back(child);
    }

    return;
  }

  operands.push_back(operand);
}

Ptr makeSimplifiedNaryExpr(GQLSimplifiedPathExpr::Kind kind, std::initializer_list<Ptr> input_operands) {
  PtrList operands;

  for (const auto &operand : input_operands) {
    appendFlattenedSimplifiedOperands(operands, kind, operand);
  }

  auto expression = make_intrusive<GQLSimplifiedPathExpr>(kind);
  expression->operands = operands;

  for (const auto &operand : expression->operands) {
    expression->children.push_back(operand);
  }

  return Ptr(expression);
}

Ptr makePathPatternAlternation(GQLPathPatternAlternation::Kind kind, const std::vector<GQLParser::PathTermContext *> &path_terms,
                               GQLParseTreeVisitor &visitor) {
  PtrList operands;
  operands.reserve(path_terms.size());

  for (auto *path_term : path_terms) {
    auto operand = castAny<Ptr>(visitor.visit(path_term));
    chassert(operand);
    chassert(operand->as<GQLPathTerm>());
    operands.push_back(operand);
  }

  return Ptr(make_intrusive<GQLPathPatternAlternation>(kind, std::move(operands)));
}

EdgeDirection getSimplifiedDefaultingDirection(GQLParser::SimplifiedPathPatternExpressionContext *context) {
  if (context->simplifiedDefaultingLeft()) {
    return EdgeDirection::Left;
  }

  if (context->simplifiedDefaultingUndirected()) {
    return EdgeDirection::Undirected;
  }

  if (context->simplifiedDefaultingRight()) {
    return EdgeDirection::Right;
  }

  if (context->simplifiedDefaultingLeftOrUndirected()) {
    return EdgeDirection::LeftOrUndirected;
  }

  if (context->simplifiedDefaultingUndirectedOrRight()) {
    return EdgeDirection::UndirectedOrRight;
  }

  if (context->simplifiedDefaultingLeftOrRight()) {
    return EdgeDirection::LeftOrRight;
  }

  return EdgeDirection::Any;
}

GQLParser::SimplifiedContentsContext *getSimplifiedDefaultingContents(GQLParser::SimplifiedPathPatternExpressionContext *context) {
  if (auto *defaulting = context->simplifiedDefaultingLeft()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingUndirected()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingRight()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingLeftOrUndirected()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingUndirectedOrRight()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingLeftOrRight()) {
    return defaulting->simplifiedContents();
  }

  if (auto *defaulting = context->simplifiedDefaultingAnyDirection()) {
    return defaulting->simplifiedContents();
  }

  return nullptr;
}

EdgeDirection getSimplifiedOverrideDirection(GQLParser::SimplifiedDirectionOverrideContext *context) {
  if (context->simplifiedOverrideLeft()) {
    return EdgeDirection::Left;
  }

  if (context->simplifiedOverrideUndirected()) {
    return EdgeDirection::Undirected;
  }

  if (context->simplifiedOverrideRight()) {
    return EdgeDirection::Right;
  }

  if (context->simplifiedOverrideLeftOrUndirected()) {
    return EdgeDirection::LeftOrUndirected;
  }

  if (context->simplifiedOverrideUndirectedOrRight()) {
    return EdgeDirection::UndirectedOrRight;
  }

  if (context->simplifiedOverrideLeftOrRight()) {
    return EdgeDirection::LeftOrRight;
  }

  if (context->simplifiedOverrideAnyDirection()) {
    return EdgeDirection::Any;
  }

  throwUnsupported("simplified direction override", context);
}

GQLParser::SimplifiedSecondaryContext *getSimplifiedOverrideSecondary(GQLParser::SimplifiedDirectionOverrideContext *context) {
  if (auto *override_context = context->simplifiedOverrideLeft()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideUndirected()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideRight()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideLeftOrUndirected()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideUndirectedOrRight()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideLeftOrRight()) {
    return override_context->simplifiedSecondary();
  }

  if (auto *override_context = context->simplifiedOverrideAnyDirection()) {
    return override_context->simplifiedSecondary();
  }

  return nullptr;
}

Ptr makeSimplifiedContents(GQLParser::SimplifiedContentsContext *context);

Ptr makeSimplifiedPrimary(GQLParser::SimplifiedPrimaryContext *context) {
  if (!context) {
    return {};
  }

  if (context->labelName()) {
    return GQLSimplifiedPathExpr::label(getText(context->labelName()));
  }

  return GQLSimplifiedPathExpr::group(makeSimplifiedContents(context->simplifiedContents()));
}

Ptr makeSimplifiedSecondary(GQLParser::SimplifiedSecondaryContext *context) {
  if (!context) {
    return {};
  }

  if (auto *negation = context->simplifiedNegation()) {
    return GQLSimplifiedPathExpr::negation(makeSimplifiedPrimary(negation->simplifiedPrimary()));
  }

  return makeSimplifiedPrimary(context->simplifiedPrimary());
}

Ptr makeSimplifiedDirectionOverride(GQLParser::SimplifiedDirectionOverrideContext *context) {
  if (!context) {
    return {};
  }

  return GQLSimplifiedPathExpr::directionOverride(getSimplifiedOverrideDirection(context),
                                                  makeSimplifiedSecondary(getSimplifiedOverrideSecondary(context)));
}

Ptr makeSimplifiedTertiary(GQLParser::SimplifiedTertiaryContext *context) {
  if (!context) {
    return {};
  }

  if (context->simplifiedDirectionOverride()) {
    return makeSimplifiedDirectionOverride(context->simplifiedDirectionOverride());
  }

  return makeSimplifiedSecondary(context->simplifiedSecondary());
}

Ptr makeSimplifiedFactorHigh(GQLParser::SimplifiedFactorHighContext *context) {
  if (!context) {
    return {};
  }

  if (context->simplifiedTertiary()) {
    return makeSimplifiedTertiary(context->simplifiedTertiary());
  }

  if (auto *quantified = context->simplifiedQuantified()) {
    return GQLSimplifiedPathExpr::repetition(makeSimplifiedTertiary(quantified->simplifiedTertiary()),
                                             makeQuantifier(quantified->graphPatternQuantifier()));
  }

  if (auto *questioned = context->simplifiedQuestioned()) {
    return GQLSimplifiedPathExpr::repetition(makeSimplifiedTertiary(questioned->simplifiedTertiary()), makeQuestionQuantifier());
  }

  return {};
}

Ptr makeSimplifiedFactorLow(GQLParser::SimplifiedFactorLowContext *context) {
  if (!context) {
    return {};
  }

  if (auto *conjunction = dynamic_cast<GQLParser::SimplifiedConjunctionLabelContext *>(context)) {
    return makeSimplifiedNaryExpr(
        GQLSimplifiedPathExpr::Kind::Conjunction,
        {makeSimplifiedFactorLow(conjunction->simplifiedFactorLow()), makeSimplifiedFactorHigh(conjunction->simplifiedFactorHigh())});
  }

  if (auto *factor_high = dynamic_cast<GQLParser::SimplifiedFactorHighLabelContext *>(context)) {
    return makeSimplifiedFactorHigh(factor_high->simplifiedFactorHigh());
  }

  return {};
}

Ptr makeSimplifiedTerm(GQLParser::SimplifiedTermContext *context) {
  if (!context) {
    return {};
  }

  if (auto *concatenation = dynamic_cast<GQLParser::SimplifiedConcatenationLabelContext *>(context)) {
    return makeSimplifiedNaryExpr(
        GQLSimplifiedPathExpr::Kind::Concatenation,
        {makeSimplifiedTerm(concatenation->simplifiedTerm()), makeSimplifiedFactorLow(concatenation->simplifiedFactorLow())});
  }

  if (auto *factor_low = dynamic_cast<GQLParser::SimplifiedFactorLowLabelContext *>(context)) {
    return makeSimplifiedFactorLow(factor_low->simplifiedFactorLow());
  }

  return {};
}

Ptr makeSimplifiedContents(GQLParser::SimplifiedContentsContext *context) {
  if (!context) {
    return {};
  }

  if (auto *path_union = context->simplifiedPathUnion()) {
    PtrList operands;

    for (auto *term : path_union->simplifiedTerm()) {
      appendFlattenedSimplifiedOperands(operands, GQLSimplifiedPathExpr::Kind::Union, makeSimplifiedTerm(term));
    }

    auto expression = make_intrusive<GQLSimplifiedPathExpr>(GQLSimplifiedPathExpr::Kind::Union);
    expression->operands = operands;
    for (const auto &operand : expression->operands) {
      expression->children.push_back(operand);
    }
    return Ptr(expression);
  }

  if (auto *alternation = context->simplifiedMultisetAlternation()) {
    PtrList operands;

    for (auto *term : alternation->simplifiedTerm()) {
      appendFlattenedSimplifiedOperands(operands, GQLSimplifiedPathExpr::Kind::MultisetAlternation, makeSimplifiedTerm(term));
    }

    auto expression = make_intrusive<GQLSimplifiedPathExpr>(GQLSimplifiedPathExpr::Kind::MultisetAlternation);
    expression->operands = operands;
    for (const auto &operand : expression->operands) {
      expression->children.push_back(operand);
    }
    return Ptr(expression);
  }

  return makeSimplifiedTerm(context->simplifiedTerm());
}

Ptr makeSimplifiedPathPattern(GQLParser::SimplifiedPathPatternExpressionContext *context) {
  if (!context) {
    return {};
  }

  auto pattern = make_intrusive<GQLSimplifiedPathPattern>(getSimplifiedDefaultingDirection(context));
  pattern->expression = makeSimplifiedContents(getSimplifiedDefaultingContents(context));
  appendClause(pattern->children, pattern->expression);
  return Ptr(pattern);
}

bool attachPathPrimaryQuantifier(const Ptr &node, Ptr quantifier) {
  if (!node || !quantifier) {
    return false;
  }

  if (auto *edge = node->as<GQLEdgePattern>()) {
    edge->quantifier = std::move(quantifier);
    appendClause(edge->children, edge->quantifier);
    return true;
  }

  if (auto *path = node->as<GQLParenthesizedPathPattern>()) {
    path->quantifier = std::move(quantifier);
    appendClause(path->children, path->quantifier);
    return true;
  }

  if (auto *simplified = node->as<GQLSimplifiedPathPattern>()) {
    simplified->quantifier = std::move(quantifier);
    appendClause(simplified->children, simplified->quantifier);
    return true;
  }

  return false;
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

template <typename T>
void fillPathSearchPrefixFields(GQLPathSearchPrefix &prefix, T *context) {
  prefix.path_mode = getPathMode(context ? context->pathMode() : nullptr);
  prefix.has_path_keyword = context && context->pathOrPaths() != nullptr;
  prefix.use_paths_keyword = context && context->pathOrPaths() && context->pathOrPaths()->PATHS();
}

Ptr makePathPatternPrefix(GQLParser::PathModePrefixContext *context) {
  if (!context) {
    return {};
  }

  auto prefix = make_intrusive<GQLPathModePrefix>();
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

  if (auto *all = search->allPathSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::All;
    fillPathSearchPrefixFields(*prefix, all);
    return Ptr(prefix);
  }

  if (auto *any = search->anyPathSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::Any;
    prefix->count = any->numberOfPaths() ? makeCountSpec(any->numberOfPaths()->nonNegativeIntegerSpecification()) : Ptr{};
    prefix->count_kind = any->numberOfPaths() ? CountKind::Paths : CountKind::None;
    fillPathSearchPrefixFields(*prefix, any);
    appendClause(prefix->children, prefix->count);
    return Ptr(prefix);
  }

  auto *shortest = search->shortestPathSearch();
  if (!shortest) {
    return {};
  }

  if (auto *all_shortest = shortest->allShortestPathSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::AllShortest;
    fillPathSearchPrefixFields(*prefix, all_shortest);
    return Ptr(prefix);
  }

  if (auto *any_shortest = shortest->anyShortestPathSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::AnyShortest;
    fillPathSearchPrefixFields(*prefix, any_shortest);
    return Ptr(prefix);
  }

  if (auto *counted_shortest = shortest->countedShortestPathSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::CountedShortest;
    prefix->count_kind = CountKind::Paths;
    prefix->count = makeCountSpec(counted_shortest->numberOfPaths()->nonNegativeIntegerSpecification());
    fillPathSearchPrefixFields(*prefix, counted_shortest);
    appendClause(prefix->children, prefix->count);
    return Ptr(prefix);
  }

  if (auto *counted_group = shortest->countedShortestGroupSearch()) {
    auto prefix = make_intrusive<GQLPathSearchPrefix>();
    prefix->search_kind = PathSearchKind::CountedShortestGroup;
    prefix->count_kind = CountKind::Groups;
    prefix->count =
        counted_group->numberOfGroups() ? makeCountSpec(counted_group->numberOfGroups()->nonNegativeIntegerSpecification()) : Ptr{};
    fillPathSearchPrefixFields(*prefix, counted_group);
    prefix->use_groups_keyword = counted_group->GROUPS() != nullptr;
    appendClause(prefix->children, prefix->count);
    return Ptr(prefix);
  }

  return {};
}

Ptr makeKeepClause(GQLParser::KeepClauseContext *context) {
  if (!context) {
    return {};
  }

  auto clause = make_intrusive<GQLKeepClause>();
  clause->path_prefix = makePathPatternPrefix(context->pathPatternPrefix());
  chassert(clause->path_prefix);
  clause->children.push_back(clause->path_prefix);
  return Ptr(clause);
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
    PtrList arguments;
    arguments.push_back(argument);
    auto set_quantifier = GQLExpr::SetQuantifier::None;

    if (auto *quantifier = general->setQuantifier()) {
      set_quantifier = quantifier->DISTINCT() ? GQLExpr::SetQuantifier::Distinct : GQLExpr::SetQuantifier::All;
    }

    return GQLExpr::functionCall(getText(general->generalSetFunctionType()), std::move(arguments), set_quantifier);
  }

  if (auto *binary = context->binarySetFunction()) {
    PtrList arguments;
    auto set_quantifier = GQLExpr::SetQuantifier::None;

    if (auto *dependent = binary->dependentValueExpression()) {
      if (auto *quantifier = dependent->setQuantifier()) {
        set_quantifier = quantifier->DISTINCT() ? GQLExpr::SetQuantifier::Distinct : GQLExpr::SetQuantifier::All;
      }

      arguments.push_back(castAny<Ptr>(visitor.visit(dependent->numericValueExpression())));
    }

    if (auto *independent = binary->independentValueExpression()) {
      arguments.push_back(castAny<Ptr>(visitor.visit(independent->numericValueExpression())));
    }

    return GQLExpr::functionCall(getText(binary->binarySetFunctionType()), std::move(arguments), set_quantifier);
  }

  throwUnsupported("aggregate function", context);
}

Ptr makeNumericValueFunction(GQLParser::NumericValueFunctionContext *numeric, GQLParseTreeVisitor &visitor) {
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

  if (auto *floor_fn = numeric->floorFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(floor_fn->numericValueExpression())));
    return GQLExpr::functionCall("FLOOR", std::move(arguments));
  }

  if (auto *ceiling_fn = numeric->ceilingFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(ceiling_fn->numericValueExpression())));
    return GQLExpr::functionCall(ceiling_fn->CEIL() ? "CEIL" : "CEILING", std::move(arguments));
  }

  if (auto *modulus = numeric->modulusExpression()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(modulus->numericValueExpressionDividend()->numericValueExpression())));
    arguments.push_back(castAny<Ptr>(visitor.visit(modulus->numericValueExpressionDivisor()->numericValueExpression())));
    return GQLExpr::functionCall("MOD", std::move(arguments));
  }

  if (auto *power = numeric->powerFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(power->numericValueExpressionBase()->numericValueExpression())));
    arguments.push_back(castAny<Ptr>(visitor.visit(power->numericValueExpressionExponent()->numericValueExpression())));
    return GQLExpr::functionCall("POWER", std::move(arguments));
  }

  if (auto *sqrt_fn = numeric->squareRoot()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(sqrt_fn->numericValueExpression())));
    return GQLExpr::functionCall("SQRT", std::move(arguments));
  }

  if (auto *trig = numeric->trigonometricFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(trig->numericValueExpression())));
    return GQLExpr::functionCall(getText(trig->trigonometricFunctionName()), std::move(arguments));
  }

  if (auto *gen_log = numeric->generalLogarithmFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(gen_log->generalLogarithmBase()->numericValueExpression())));
    arguments.push_back(castAny<Ptr>(visitor.visit(gen_log->generalLogarithmArgument()->numericValueExpression())));
    return GQLExpr::functionCall("LOG", std::move(arguments));
  }

  if (auto *common_log = numeric->commonLogarithm()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(common_log->numericValueExpression())));
    return GQLExpr::functionCall("LOG10", std::move(arguments));
  }

  if (auto *nat_log = numeric->naturalLogarithm()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(nat_log->numericValueExpression())));
    return GQLExpr::functionCall("LN", std::move(arguments));
  }

  if (auto *exp_fn = numeric->exponentialFunction()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(exp_fn->numericValueExpression())));
    return GQLExpr::functionCall("EXP", std::move(arguments));
  }

  throwUnsupported("numeric value function", numeric);
}

Ptr makeCharacterOrByteStringFunction(GQLParser::CharacterOrByteStringFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (auto *sub = context->subCharacterOrByteString()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(sub->valueExpression())));
    arguments.push_back(castAny<Ptr>(visitor.visit(sub->stringLength()->numericValueExpression())));
    return GQLExpr::functionCall(sub->LEFT() ? "LEFT" : "RIGHT", std::move(arguments));
  }

  if (auto *fold = context->foldCharacterString()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(fold->valueExpression())));
    return GQLExpr::functionCall(fold->UPPER() ? "UPPER" : "LOWER", std::move(arguments));
  }

  if (auto *multi_trim = context->trimMultiCharacterCharacterString()) {
    PtrList arguments;
    for (auto *expr : multi_trim->valueExpression()) arguments.push_back(castAny<Ptr>(visitor.visit(expr)));

    String name;
    if (multi_trim->BTRIM())
      name = "BTRIM";
    else if (multi_trim->LTRIM())
      name = "LTRIM";
    else
      name = "RTRIM";
    return GQLExpr::functionCall(name, std::move(arguments));
  }

  if (auto *normalize = context->normalizeCharacterString()) {
    PtrList arguments;
    arguments.push_back(castAny<Ptr>(visitor.visit(normalize->valueExpression())));
    if (normalize->normalForm()) arguments.push_back(GQLExpr::literal(getText(normalize->normalForm())));
    return GQLExpr::functionCall("NORMALIZE", std::move(arguments));
  }

  if (auto *single_trim = context->trimSingleCharacterOrByteString()) {
    auto *operands = single_trim->trimOperands();
    auto source = castAny<Ptr>(visitor.visit(operands->trimCharacterOrByteStringSource()->valueExpression()));

    GQLExpr::TrimSpec spec = GQLExpr::TrimSpec::None;
    if (auto *ts = operands->trimSpecification()) {
      if (ts->LEADING())
        spec = GQLExpr::TrimSpec::Leading;
      else if (ts->TRAILING())
        spec = GQLExpr::TrimSpec::Trailing;
      else
        spec = GQLExpr::TrimSpec::Both;
    }

    Ptr trim_char;
    if (auto *tc = operands->trimCharacterOrByteString()) trim_char = castAny<Ptr>(visitor.visit(tc->valueExpression()));

    return GQLExpr::trimString(std::move(source), spec, std::move(trim_char));
  }

  throwUnsupported("character or byte string function", context);
}

Ptr makeDatetimeValueFunction(GQLParser::DatetimeValueFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (auto *date_fn = context->dateFunction()) {
    if (date_fn->CURRENT_DATE()) return GQLExpr::bareKeywordFunction("CURRENT_DATE");
    PtrList args;
    if (auto *params = date_fn->dateFunctionParameters()) {
      if (auto *rc = params->recordConstructor())
        args.push_back(makeRecordConstructor(rc, visitor));
      else
        args.push_back(GQLExpr::literal(getText(params)));
    }
    return GQLExpr::functionCall("DATE", std::move(args));
  }

  if (auto *time_fn = context->timeFunction()) {
    if (time_fn->CURRENT_TIME()) return GQLExpr::bareKeywordFunction("CURRENT_TIME");
    PtrList args;
    if (auto *params = time_fn->timeFunctionParameters()) {
      if (auto *rc = params->recordConstructor())
        args.push_back(makeRecordConstructor(rc, visitor));
      else
        args.push_back(GQLExpr::literal(getText(params)));
    }
    return GQLExpr::functionCall("ZONED_TIME", std::move(args));
  }

  if (auto *datetime_fn = context->datetimeFunction()) {
    if (datetime_fn->CURRENT_TIMESTAMP()) return GQLExpr::bareKeywordFunction("CURRENT_TIMESTAMP");
    PtrList args;
    if (auto *params = datetime_fn->datetimeFunctionParameters()) {
      if (auto *rc = params->recordConstructor())
        args.push_back(makeRecordConstructor(rc, visitor));
      else
        args.push_back(GQLExpr::literal(getText(params)));
    }
    return GQLExpr::functionCall("ZONED_DATETIME", std::move(args));
  }

  if (auto *localtime_fn = context->localtimeFunction()) {
    if (!localtime_fn->LEFT_PAREN()) return GQLExpr::bareKeywordFunction("LOCAL_TIME");
    PtrList args;
    if (auto *params = localtime_fn->timeFunctionParameters()) {
      if (auto *rc = params->recordConstructor())
        args.push_back(makeRecordConstructor(rc, visitor));
      else
        args.push_back(GQLExpr::literal(getText(params)));
    }
    return GQLExpr::functionCall("LOCAL_TIME", std::move(args));
  }

  if (auto *localdatetime_fn = context->localdatetimeFunction()) {
    if (localdatetime_fn->LOCAL_TIMESTAMP()) return GQLExpr::bareKeywordFunction("LOCAL_TIMESTAMP");
    PtrList args;
    if (auto *params = localdatetime_fn->datetimeFunctionParameters()) {
      if (auto *rc = params->recordConstructor())
        args.push_back(makeRecordConstructor(rc, visitor));
      else
        args.push_back(GQLExpr::literal(getText(params)));
    }
    return GQLExpr::functionCall("LOCAL_DATETIME", std::move(args));
  }

  throwUnsupported("datetime value function", context);
}

Ptr makeDurationValueFunction(GQLParser::DurationValueFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (auto *duration_fn = context->durationFunction()) {
    PtrList args;
    auto *params = duration_fn->durationFunctionParameters();
    if (auto *rc = params->recordConstructor())
      args.push_back(makeRecordConstructor(rc, visitor));
    else
      args.push_back(GQLExpr::literal(getText(params)));
    return GQLExpr::functionCall("DURATION", std::move(args));
  }

  if (auto *absolute = context->absoluteValueExpression()) {
    PtrList args;
    args.push_back(castAny<Ptr>(visitor.visit(absolute->valueExpression())));
    return GQLExpr::functionCall("ABS", std::move(args));
  }

  throwUnsupported("duration value function", context);
}

Ptr makeListValueFunction(GQLParser::ListValueFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (auto *elements_fn = context->elementsFunction()) {
    PtrList args;
    args.push_back(castAny<Ptr>(visitor.visit(elements_fn->pathValueExpression()->valueExpression())));
    return GQLExpr::functionCall("ELEMENTS", std::move(args));
  }

  if (auto *trim_list = context->trimListFunction()) {
    PtrList args;
    args.push_back(castAny<Ptr>(visitor.visit(trim_list->listValueExpression()->valueExpression())));
    args.push_back(castAny<Ptr>(visitor.visit(trim_list->numericValueExpression())));
    return GQLExpr::functionCall("TRIM", std::move(args));
  }

  throwUnsupported("list value function", context);
}

Ptr makeValueFunction(GQLParser::ValueFunctionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) return {};

  if (auto *numeric = context->numericValueFunction()) return makeNumericValueFunction(numeric, visitor);

  if (auto *char_fn = context->characterOrByteStringFunction()) return makeCharacterOrByteStringFunction(char_fn, visitor);

  if (auto *datetime = context->datetimeValueFunction()) return makeDatetimeValueFunction(datetime, visitor);

  if (auto *duration = context->durationValueFunction()) return makeDurationValueFunction(duration, visitor);

  if (auto *list_fn = context->listValueFunction()) return makeListValueFunction(list_fn, visitor);

  if (auto *dt_sub = context->datetimeSubtraction()) {
    auto *params = dt_sub->datetimeSubtractionParameters();
    auto left = castAny<Ptr>(visitor.visit(params->datetimeValueExpression1()->datetimeValueExpression()->valueExpression()));
    auto right = castAny<Ptr>(visitor.visit(params->datetimeValueExpression2()->datetimeValueExpression()->valueExpression()));

    GQLExpr::TemporalQualifier qualifier = GQLExpr::TemporalQualifier::None;
    if (auto *tq = dt_sub->temporalDurationQualifier()) {
      qualifier = tq->YEAR() ? GQLExpr::TemporalQualifier::YearToMonth : GQLExpr::TemporalQualifier::DayToSecond;
    }
    return GQLExpr::durationBetween(std::move(left), std::move(right), qualifier);
  }

  throwUnsupported("value function", context);
}

Ptr makeResultExpr(GQLParser::ResultContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) return {};

  if (context->nullLiteral()) return GQLExpr::specialValue("NULL");

  if (auto *result_expr = context->resultExpression()) return castAny<Ptr>(visitor.visit(result_expr->valueExpression()));

  throwUnsupported("result expression", context);
}

Ptr makeCaseExpression(GQLParser::CaseExpressionContext *context, GQLParseTreeVisitor &visitor);
Ptr makeCastSpecification(GQLParser::CastSpecificationContext *context, GQLParseTreeVisitor &visitor);

Ptr makeValueQueryExpr(GQLParser::ValueQueryExpressionContext *context, GQLParseTreeVisitor &visitor) {
  auto subquery = castAny<Ptr>(visitor.visit(context->nestedQuerySpecification()));
  return GQLExpr::valueQuery(std::move(subquery));
}

Ptr makeLetValueExpr(GQLParser::LetValueExpressionContext *context, GQLParseTreeVisitor &visitor) {
  PtrList bindings;
  for (auto *def : context->letVariableDefinitionList()->letVariableDefinition()) {
    Ptr assignment;
    if (auto *value_definition = def->valueVariableDefinition()) {
      Ptr value;
      String raw_type;
      if (auto *initializer = value_definition->optTypedValueInitializer()) {
        if (initializer->valueType()) raw_type = getText(initializer->valueType());
        if (initializer->valueInitializer()) value = castAny<Ptr>(visitor.visit(initializer->valueInitializer()->valueExpression()));
      }
      assignment =
          Ptr(make_intrusive<GQLAssignmentItem>(getText(value_definition->bindingVariable()), std::move(value), true, std::move(raw_type)));
    } else {
      assignment =
          Ptr(make_intrusive<GQLAssignmentItem>(getText(def->bindingVariable()), castAny<Ptr>(visitor.visit(def->valueExpression()))));
    }
    bindings.push_back(std::move(assignment));
  }
  auto body = castAny<Ptr>(visitor.visit(context->valueExpression()));
  return GQLExpr::letExpr(std::move(bindings), std::move(body));
}

Ptr makePathValueConstructorExpr(GQLParser::PathValueConstructorContext *context, GQLParseTreeVisitor &visitor) {
  auto *by_enum = context->pathValueConstructorByEnumeration();
  auto *elem_list = by_enum->pathElementList();
  PtrList elements;
  elements.push_back(
      castAny<Ptr>(visitor.visit(elem_list->pathElementListStart()->nodeReferenceValueExpression()->valueExpressionPrimary())));
  for (auto *step : elem_list->pathElementListStep()) {
    elements.push_back(castAny<Ptr>(visitor.visit(step->edgeReferenceValueExpression()->valueExpressionPrimary())));
    elements.push_back(castAny<Ptr>(visitor.visit(step->nodeReferenceValueExpression()->valueExpressionPrimary())));
  }
  return GQLExpr::pathConstructor(std::move(elements));
}

template <typename Context>
Ptr tryMakeStructuredValuePrimary(Context *context, GQLParseTreeVisitor &visitor) {
  if (auto *value_query = context->valueQueryExpression()) return makeValueQueryExpr(value_query, visitor);
  if (auto *let_expr = context->letValueExpression()) return makeLetValueExpr(let_expr, visitor);
  if (auto *path_ctor = context->pathValueConstructor()) return makePathValueConstructorExpr(path_ctor, visitor);
  return {};
}

Ptr makeNpvepExpr(GQLParser::NonParenthesizedValueExpressionPrimaryContext *npvep, GQLParseTreeVisitor &visitor) {
  if (auto *bvr = npvep->bindingVariableReference()) return GQLExpr::identifier(getText(bvr->bindingVariable()));

  if (auto *special = npvep->nonParenthesizedValueExpressionPrimarySpecialCase()) {
    if (special->unsignedValueSpecification()) return castAny<Ptr>(visitor.visit(special->unsignedValueSpecification()));
    if (special->aggregateFunction()) return makeAggregateFunction(special->aggregateFunction(), visitor);
    if (special->caseExpression()) return makeCaseExpression(special->caseExpression(), visitor);
    if (special->castSpecification()) return makeCastSpecification(special->castSpecification(), visitor);
    if (special->element_idFunction()) {
      PtrList args;
      args.push_back(GQLExpr::identifier(getElementVariableName(special->element_idFunction()->elementVariableReference())));
      return GQLExpr::functionCall("ELEMENT_ID", std::move(args));
    }
    if (special->propertyName()) {
      auto base = castAny<Ptr>(visitor.visit(special->valueExpressionPrimary()));
      return GQLExpr::property(std::move(base), getText(special->propertyName()->identifier()));
    }
    if (auto structured = tryMakeStructuredValuePrimary(special, visitor)) return structured;
    throwUnsupported("non-parenthesized value expression primary special case", special);
  }

  throwUnsupported("non-parenthesized value expression primary", npvep);
}

Ptr makeWhenOperandExpr(GQLParser::WhenOperandContext *wo, GQLParseTreeVisitor &visitor, Ptr case_operand = {}) {
  if (auto *npvep = wo->nonParenthesizedValueExpressionPrimary()) return makeNpvepExpr(npvep, visitor);

  auto cloneLeft = [&]() -> Ptr { return case_operand ? case_operand->clone() : GQLExpr::rawText("?"); };

  if (wo->compOp()) {
    auto right = castAny<Ptr>(visitor.visit(wo->valueExpression()));
    return GQLExpr::binaryOp(getText(wo->compOp()), cloneLeft(), std::move(right));
  }

  if (auto *null_part = wo->nullPredicatePart2()) {
    const String op = null_part->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, cloneLeft(), GQLExpr::specialValue("NULL"));
  }

  if (auto *vt_part = wo->valueTypePredicatePart2()) {
    const String op = vt_part->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, cloneLeft(), GQLExpr::literal(getText(vt_part->valueType())));
  }

  if (auto *dir_part = wo->directedPredicatePart2()) {
    const String op = dir_part->NOT() ? "IS NOT" : "IS";
    return GQLExpr::binaryOp(op, cloneLeft(), GQLExpr::literal("DIRECTED"));
  }

  if (auto *label_part = wo->labeledPredicatePart2()) {
    auto *labeled_or_colon = label_part->isLabeledOrColon();
    String op = ":";
    if (labeled_or_colon->IS()) op = labeled_or_colon->NOT() ? "IS NOT LABELED" : "IS LABELED";
    return GQLExpr::binaryOp(op, cloneLeft(), castAny<Ptr>(visitor.visit(label_part->labelExpression())));
  }

  if (auto *src_part = wo->sourcePredicatePart2()) {
    const String op = src_part->NOT() ? "IS NOT SOURCE OF" : "IS SOURCE OF";
    return GQLExpr::binaryOp(op, cloneLeft(),
                             GQLExpr::identifier(getElementVariableName(src_part->edgeReference()->elementVariableReference())));
  }

  if (auto *dst_part = wo->destinationPredicatePart2()) {
    const String op = dst_part->NOT() ? "IS NOT DESTINATION OF" : "IS DESTINATION OF";
    return GQLExpr::binaryOp(op, cloneLeft(),
                             GQLExpr::identifier(getElementVariableName(dst_part->edgeReference()->elementVariableReference())));
  }

  if (auto *np = wo->normalizedPredicatePart2()) {
    bool negated = np->NOT() != nullptr;
    auto form = GQLExpr::NormalForm::None;
    if (auto *nf = np->normalForm()) {
      if (nf->NFC())
        form = GQLExpr::NormalForm::NFC;
      else if (nf->NFD())
        form = GQLExpr::NormalForm::NFD;
      else if (nf->NFKC())
        form = GQLExpr::NormalForm::NFKC;
      else if (nf->NFKD())
        form = GQLExpr::NormalForm::NFKD;
    }
    String op = negated ? "IS NOT" : "IS";
    String right_text;
    if (form != GQLExpr::NormalForm::None) {
      static const char *form_names[] = {"", "NFC", "NFD", "NFKC", "NFKD"};
      right_text = form_names[static_cast<UInt8>(form)];
      right_text += " NORMALIZED";
    } else {
      right_text = "NORMALIZED";
    }
    return GQLExpr::binaryOp(op, cloneLeft(), GQLExpr::literal(right_text));
  }

  throwUnsupported("when operand", wo);
}

Ptr makeCaseExpression(GQLParser::CaseExpressionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) return {};

  if (auto *abbreviation = context->caseAbbreviation()) {
    PtrList arguments;
    for (auto *value_expr : abbreviation->valueExpression()) arguments.push_back(castAny<Ptr>(visitor.visit(value_expr)));

    String name = abbreviation->NULLIF() ? "NULLIF" : "COALESCE";
    return GQLExpr::functionCall(name, std::move(arguments));
  }

  auto *spec = context->caseSpecification();
  if (!spec) throwUnsupported("case expression", context);

  if (auto *searched = spec->searchedCase()) {
    auto expr = make_intrusive<GQLCaseExpr>(GQLCaseExpr::Form::Searched);

    for (auto *clause : searched->searchedWhenClause()) {
      auto when_expr = castAny<Ptr>(visitor.visit(clause->searchCondition()));
      auto then_expr = makeResultExpr(clause->result(), visitor);
      expr->when_operands.push_back(when_expr);
      expr->then_results.push_back(then_expr);
      if (when_expr) expr->children.push_back(when_expr);
      if (then_expr) expr->children.push_back(then_expr);
    }

    if (auto *else_clause = searched->elseClause()) {
      expr->else_result = makeResultExpr(else_clause->result(), visitor);
      if (expr->else_result) expr->children.push_back(expr->else_result);
    }

    return Ptr(expr);
  }

  if (auto *simple = spec->simpleCase()) {
    auto expr = make_intrusive<GQLCaseExpr>(GQLCaseExpr::Form::Simple);

    if (auto *operand = simple->caseOperand()) {
      if (auto *elem_ref = operand->elementVariableReference())
        expr->operand = GQLExpr::identifier(getElementVariableName(elem_ref));
      else if (auto *npvep = operand->nonParenthesizedValueExpressionPrimary())
        expr->operand = makeNpvepExpr(npvep, visitor);
      if (expr->operand) expr->children.push_back(expr->operand);
    }

    for (auto *clause : simple->simpleWhenClause()) {
      Ptr when_expr;
      if (auto *when_list = clause->whenOperandList()) {
        auto operands = when_list->whenOperand();
        if (operands.size() == 1) {
          when_expr = makeWhenOperandExpr(operands.front(), visitor, expr->operand);
        } else {
          PtrList items;
          for (auto *wo : operands) items.push_back(makeWhenOperandExpr(wo, visitor, expr->operand));
          when_expr = GQLExpr::exprList(std::move(items));
        }
      }

      auto then_expr = makeResultExpr(clause->result(), visitor);
      expr->when_operands.push_back(when_expr);
      expr->then_results.push_back(then_expr);
      if (when_expr) expr->children.push_back(when_expr);
      if (then_expr) expr->children.push_back(then_expr);
    }

    if (auto *else_clause = simple->elseClause()) {
      expr->else_result = makeResultExpr(else_clause->result(), visitor);
      if (expr->else_result) expr->children.push_back(expr->else_result);
    }

    return Ptr(expr);
  }

  throwUnsupported("case specification", context);
}

Ptr makeCastSpecification(GQLParser::CastSpecificationContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) return {};

  Ptr operand;
  if (auto *cast_operand = context->castOperand()) {
    if (cast_operand->nullLiteral())
      operand = GQLExpr::specialValue("NULL");
    else if (cast_operand->valueExpression())
      operand = castAny<Ptr>(visitor.visit(cast_operand->valueExpression()));
  }

  String target_type;
  if (auto *target = context->castTarget()) target_type = getText(target);

  return GQLExpr::castExpr(std::move(operand), target_type);
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

Ptr makeLabelSetSpecification(GQLParser::LabelSetSpecificationContext *context) {
  if (!context) return {};

  auto labels = context->labelName();
  if (labels.empty()) return {};

  Ptr result = GQLLabelExpression::name(getText(labels[0]->identifier()));

  for (size_t i = 1; i < labels.size(); ++i) {
    result = GQLLabelExpression::binary(GQLLabelExpression::Kind::Conjunction, std::move(result),
                                        GQLLabelExpression::name(getText(labels[i]->identifier())));
  }

  return result;
}

ElementPatternParts makeInsertElementFiller(GQLParser::InsertElementPatternFillerContext *filler, GQLParseTreeVisitor &visitor) {
  ElementPatternParts parts;
  if (!filler) return parts;

  if (filler->elementVariableDeclaration()) {
    parts.variable = GQLExpr::identifier(getText(filler->elementVariableDeclaration()->elementVariable()->bindingVariable()));
  }

  if (auto *label_and_props = filler->labelAndPropertySetSpecification()) {
    if (auto *label_spec = label_and_props->labelSetSpecification()) {
      parts.label_expression = makeLabelSetSpecification(label_spec);
    }

    if (auto *prop_spec = label_and_props->elementPropertySpecification()) {
      parts.properties = castAny<Ptr>(visitor.visit(prop_spec));
    }
  }

  return parts;
}

Ptr makeInsertClause(GQLParser::InsertStatementContext *context, GQLParseTreeVisitor &visitor) {
  auto clause = make_intrusive<GQLInsertClause>();
  auto *graph_pattern = context->insertGraphPattern();
  if (!graph_pattern) return Ptr(clause);

  for (auto *path : graph_pattern->insertPathPatternList()->insertPathPattern()) {
    auto path_pattern = make_intrusive<GQLInsertPathPattern>();
    auto nodes = path->insertNodePattern();
    auto edges = path->insertEdgePattern();

    for (size_t i = 0; i < nodes.size(); ++i) {
      auto node = makeNodeOrEdgePattern(makeInsertElementFiller(nodes[i]->insertElementPatternFiller(), visitor), false);
      path_pattern->elements.push_back(node);
      if (node) path_pattern->children.push_back(node);

      if (i < edges.size()) {
        auto *edge_ctx = edges[i];
        EdgeDirection direction = EdgeDirection::Right;
        GQLParser::InsertElementPatternFillerContext *edge_filler = nullptr;

        if (auto *left = edge_ctx->insertEdgePointingLeft()) {
          direction = EdgeDirection::Left;
          edge_filler = left->insertElementPatternFiller();
        } else if (auto *right = edge_ctx->insertEdgePointingRight()) {
          direction = EdgeDirection::Right;
          edge_filler = right->insertElementPatternFiller();
        } else if (auto *undirected = edge_ctx->insertEdgeUndirected()) {
          direction = EdgeDirection::Undirected;
          edge_filler = undirected->insertElementPatternFiller();
        }

        auto edge = makeNodeOrEdgePattern(makeInsertElementFiller(edge_filler, visitor), true, direction);
        path_pattern->elements.push_back(edge);
        if (edge) path_pattern->children.push_back(edge);
      }
    }

    clause->path_patterns.push_back(Ptr(path_pattern));
    clause->children.push_back(Ptr(path_pattern));
  }

  return Ptr(clause);
}

Ptr makeSetClause(GQLParser::SetStatementContext *context, GQLParseTreeVisitor &visitor) {
  auto clause = make_intrusive<GQLSetClause>();

  for (auto *item_ctx : context->setItemList()->setItem()) {
    if (auto *prop = item_ctx->setPropertyItem()) {
      String var_name = getText(prop->bindingVariableReference()->bindingVariable());
      String prop_name = getText(prop->propertyName()->identifier());
      auto set_item = make_intrusive<GQLSetItem>(GQLSetItem::Kind::Property, std::move(var_name), std::move(prop_name));
      set_item->value = castAny<Ptr>(visitor.visit(prop->valueExpression()));
      if (set_item->value) set_item->children.push_back(set_item->value);
      clause->items.push_back(Ptr(set_item));
      clause->children.push_back(Ptr(set_item));
    } else if (auto *all_props = item_ctx->setAllPropertiesItem()) {
      String var_name = getText(all_props->bindingVariableReference()->bindingVariable());
      auto set_item = make_intrusive<GQLSetItem>(GQLSetItem::Kind::AllProperties, std::move(var_name));
      PtrList kv_items;
      if (auto *kv_list = all_props->propertyKeyValuePairList()) kv_items = castAny<PtrList>(visitor.visit(kv_list));
      set_item->value = Ptr(make_intrusive<GQLPropertyMap>(std::move(kv_items)));
      if (set_item->value) set_item->children.push_back(set_item->value);
      clause->items.push_back(Ptr(set_item));
      clause->children.push_back(Ptr(set_item));
    } else if (auto *label = item_ctx->setLabelItem()) {
      String var_name = getText(label->bindingVariableReference()->bindingVariable());
      String label_name = getText(label->labelName()->identifier());
      auto set_item = make_intrusive<GQLSetItem>(GQLSetItem::Kind::Label, std::move(var_name), std::move(label_name));
      set_item->use_is = label->isOrColon()->IS() != nullptr;
      clause->items.push_back(Ptr(set_item));
      clause->children.push_back(Ptr(set_item));
    }
  }

  return Ptr(clause);
}

Ptr makeRemoveClause(GQLParser::RemoveStatementContext *context) {
  auto clause = make_intrusive<GQLRemoveClause>();

  for (auto *item_ctx : context->removeItemList()->removeItem()) {
    if (auto *prop = item_ctx->removePropertyItem()) {
      String var_name = getText(prop->bindingVariableReference()->bindingVariable());
      String prop_name = getText(prop->propertyName()->identifier());
      auto remove_item = make_intrusive<GQLRemoveItem>(GQLRemoveItem::Kind::Property, std::move(var_name), std::move(prop_name));
      clause->items.push_back(Ptr(remove_item));
      clause->children.push_back(Ptr(remove_item));
    } else if (auto *label = item_ctx->removeLabelItem()) {
      String var_name = getText(label->bindingVariableReference()->bindingVariable());
      String label_name = getText(label->labelName()->identifier());
      bool use_is = label->isOrColon()->IS() != nullptr;
      auto remove_item = make_intrusive<GQLRemoveItem>(GQLRemoveItem::Kind::Label, std::move(var_name), std::move(label_name), use_is);
      clause->items.push_back(Ptr(remove_item));
      clause->children.push_back(Ptr(remove_item));
    }
  }

  return Ptr(clause);
}

Ptr makeDeleteClause(GQLParser::DeleteStatementContext *context, GQLParseTreeVisitor &visitor) {
  auto detach_mode = GQLDeleteClause::DetachMode::None;
  if (context->DETACH())
    detach_mode = GQLDeleteClause::DetachMode::Detach;
  else if (context->NODETACH())
    detach_mode = GQLDeleteClause::DetachMode::NoDetach;

  auto clause = make_intrusive<GQLDeleteClause>(detach_mode);

  for (auto *item_ctx : context->deleteItemList()->deleteItem()) {
    auto value = castAny<Ptr>(visitor.visit(item_ctx->valueExpression()));
    clause->items.push_back(value);
    if (value) clause->children.push_back(value);
  }

  return Ptr(clause);
}

Ptr makePrimitiveDataModifyingClause(GQLParser::PrimitiveDataModifyingStatementContext *context, GQLParseTreeVisitor &visitor) {
  if (auto *insert_stmt = context->insertStatement()) return makeInsertClause(insert_stmt, visitor);
  if (auto *set_stmt = context->setStatement()) return makeSetClause(set_stmt, visitor);
  if (auto *remove_stmt = context->removeStatement()) return makeRemoveClause(remove_stmt);
  if (auto *delete_stmt = context->deleteStatement()) return makeDeleteClause(delete_stmt, visitor);
  return {};
}

Ptr makeCallClause(GQLParser::CallProcedureStatementContext *statement, GQLParseTreeVisitor &visitor) {
  auto *procedure_call = statement->procedureCall();

  if (auto *named = procedure_call->namedProcedureCall()) {
    auto clause = make_intrusive<GQLCallNamedClause>();
    clause->optional = statement->OPTIONAL() != nullptr;
    clause->procedure = GQLExpr::identifier(getText(named->procedureReference()));

    if (auto *argument_list = named->procedureArgumentList()) {
      for (auto *argument : argument_list->procedureArgument())
        clause->arguments.push_back(castAny<Ptr>(visitor.visit(argument->valueExpression())));
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
    auto clause = make_intrusive<GQLCallInlineClause>();
    clause->optional = statement->OPTIONAL() != nullptr;
    clause->variable_scope = makeCallVariableScopeClause(inline_call->variableScopeClause());
    clause->subquery =
        visitor.buildSubquery(inline_call->nestedProcedureSpecification()->procedureSpecification()->procedureBody(), inline_call);
    appendClause(clause->children, clause->variable_scope);
    appendClause(clause->children, clause->subquery);
    return Ptr(clause);
  }

  throwUnsupported("procedure call", procedure_call);
}

PtrList makeDataAccessingClauses(GQLParser::SimpleLinearDataAccessingStatementContext *context, GQLParseTreeVisitor &visitor) {
  PtrList clauses;
  if (!context) return clauses;

  for (auto *stmt : context->simpleDataAccessingStatement()) {
    if (auto *query_stmt = stmt->simpleQueryStatement()) {
      clauses.push_back(castAny<Ptr>(visitor.visit(query_stmt)));
    } else if (auto *modify_stmt = stmt->simpleDataModifyingStatement()) {
      if (auto *primitive = modify_stmt->primitiveDataModifyingStatement()) {
        appendClause(clauses, makePrimitiveDataModifyingClause(primitive, visitor));
      } else if (auto *call_stmt = modify_stmt->callDataModifyingProcedureStatement()) {
        clauses.push_back(makeCallClause(call_stmt->callProcedureStatement(), visitor));
      }
    }
  }

  return clauses;
}

Ptr makeLinearDataModifyingQuery(GQLParser::LinearDataModifyingStatementContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) return {};

  PtrList clauses;

  if (auto *focused = context->focusedLinearDataModifyingStatement()) {
    if (auto *body = focused->focusedLinearDataModifyingStatementBody()) {
      appendClause(clauses, makeUseGraphClause(body->useGraphClause(), visitor));
      appendClauseList(clauses, makeDataAccessingClauses(body->simpleLinearDataAccessingStatement(), visitor));
      if (body->primitiveResultStatement()) {
        appendQueryResultClause(clauses, castAny<Ptr>(visitor.visit(body->primitiveResultStatement())));
      }
    } else if (auto *nested = focused->focusedNestedDataModifyingProcedureSpecification()) {
      appendClause(clauses, makeUseGraphClause(nested->useGraphClause(), visitor));
      auto *nested_spec = nested->nestedDataModifyingProcedureSpecification();
      clauses.push_back(visitor.buildSubquery(nested_spec->procedureBody(), nested_spec));
    }
  } else if (auto *ambient = context->ambientLinearDataModifyingStatement()) {
    if (auto *body = ambient->ambientLinearDataModifyingStatementBody()) {
      appendClauseList(clauses, makeDataAccessingClauses(body->simpleLinearDataAccessingStatement(), visitor));
      if (body->primitiveResultStatement()) {
        appendQueryResultClause(clauses, castAny<Ptr>(visitor.visit(body->primitiveResultStatement())));
      }
    } else if (auto *nested = ambient->nestedDataModifyingProcedureSpecification()) {
      clauses.push_back(visitor.buildSubquery(nested->procedureBody(), nested));
    }
  }

  if (clauses.empty()) return {};

  return makeSingleQuery(std::move(clauses));
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

    if (statement->linearDataModifyingStatement()) return makeLinearDataModifyingQuery(statement->linearDataModifyingStatement(), *this);

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
    auto *match_clause = clause ? clause->as<GQLMatchClause>() : nullptr;
    chassert(match_clause);

    if (match_clause) match_clause->optional = true;

    return clause;
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

  chassert(false);
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
  result.keep_clause = makeKeepClause(context->keepClause());
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
  auto pattern = make_intrusive<GQLPathPattern>();

  if (context->pathVariableDeclaration()) {
    pattern->variable = GQLExpr::identifier(getText(context->pathVariableDeclaration()->pathVariable()->bindingVariable()));
    appendClause(pattern->children, pattern->variable);
  }

  if (context->pathPatternPrefix()) {
    pattern->prefix = makePathPatternPrefix(context->pathPatternPrefix());
    appendClause(pattern->children, pattern->prefix);
  }

  pattern->expression = castAny<Ptr>(visit(context->pathPatternExpression()));
  appendClause(pattern->children, pattern->expression);

  return Ptr(pattern);
}

std::any GQLParseTreeVisitor::visitPpePathTerm(GQLParser::PpePathTermContext *context) { return castAny<Ptr>(visit(context->pathTerm())); }

std::any GQLParseTreeVisitor::visitPpeMultisetAlternation(GQLParser::PpeMultisetAlternationContext *context) {
  return makePathPatternAlternation(GQLPathPatternAlternation::Kind::MultisetAlternation, context->pathTerm(), *this);
}

std::any GQLParseTreeVisitor::visitPpePatternUnion(GQLParser::PpePatternUnionContext *context) {
  return makePathPatternAlternation(GQLPathPatternAlternation::Kind::Union, context->pathTerm(), *this);
}

std::any GQLParseTreeVisitor::visitPathTerm(GQLParser::PathTermContext *context) {
  PtrList factors;

  for (auto *factor : context->pathFactor()) {
    factors.push_back(castAny<Ptr>(visit(factor)));
  }

  return Ptr(make_intrusive<GQLPathTerm>(std::move(factors)));
}

std::any GQLParseTreeVisitor::visitPfPathPrimary(GQLParser::PfPathPrimaryContext *context) {
  return castAny<Ptr>(visit(context->pathPrimary()));
}

std::any GQLParseTreeVisitor::visitPfQuantifiedPathPrimary(GQLParser::PfQuantifiedPathPrimaryContext *context) {
  auto node = castAny<Ptr>(visit(context->pathPrimary()));
  if (attachPathPrimaryQuantifier(node, makeQuantifier(context->graphPatternQuantifier()))) {
    return node;
  }

  auto wrapper = make_intrusive<GQLQuantifiedPathPrimary>();
  wrapper->operand = node;
  wrapper->quantifier = makeQuantifier(context->graphPatternQuantifier());
  appendClause(wrapper->children, wrapper->operand);
  appendClause(wrapper->children, wrapper->quantifier);
  return Ptr(wrapper);
}

std::any GQLParseTreeVisitor::visitPfQuestionedPathPrimary(GQLParser::PfQuestionedPathPrimaryContext *context) {
  auto node = castAny<Ptr>(visit(context->pathPrimary()));
  if (attachPathPrimaryQuantifier(node, makeQuestionQuantifier())) {
    return node;
  }

  auto wrapper = make_intrusive<GQLQuantifiedPathPrimary>();
  wrapper->operand = node;
  wrapper->quantifier = makeQuestionQuantifier();
  appendClause(wrapper->children, wrapper->operand);
  appendClause(wrapper->children, wrapper->quantifier);
  return Ptr(wrapper);
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

  pattern->expression = castAny<Ptr>(visit(pattern_context->pathPatternExpression()));
  appendClause(pattern->children, pattern->expression);

  if (pattern_context->parenthesizedPathPatternWhereClause()) {
    pattern->where =
        Ptr(make_intrusive<GQLWhereClause>(castAny<Ptr>(visit(pattern_context->parenthesizedPathPatternWhereClause()->searchCondition()))));
    appendClause(pattern->children, pattern->where);
  }

  return Ptr(pattern);
}

std::any GQLParseTreeVisitor::visitPpSimplifiedPathPatternExpression(GQLParser::PpSimplifiedPathPatternExpressionContext *context) {
  return makeSimplifiedPathPattern(context->simplifiedPathPatternExpression());
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
  return GQLExpr::binaryOp(op, castAny<Ptr>(visit(context->valueExpression())), GQLExpr::specialValue(getText(context->truthValue())));
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
    return GQLExpr::binaryOp(op, castAny<Ptr>(visit(null_predicate->valueExpressionPrimary())), GQLExpr::specialValue("NULL"));
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

std::any GQLParseTreeVisitor::visitNormalizedPredicateExprAlt(GQLParser::NormalizedPredicateExprAltContext *context) {
  auto operand = castAny<Ptr>(visit(context->valueExpression()));
  auto *part2 = context->normalizedPredicatePart2();
  bool negated = part2->NOT() != nullptr;
  auto form = GQLExpr::NormalForm::None;

  if (auto *nf = part2->normalForm()) {
    if (nf->NFC())
      form = GQLExpr::NormalForm::NFC;
    else if (nf->NFD())
      form = GQLExpr::NormalForm::NFD;
    else if (nf->NFKC())
      form = GQLExpr::NormalForm::NFKC;
    else if (nf->NFKD())
      form = GQLExpr::NormalForm::NFKD;
  }

  return GQLExpr::normalizedPredicate(std::move(operand), negated, form);
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

  if (auto *element_id_function = context->element_idFunction()) {
    PtrList arguments;
    arguments.push_back(GQLExpr::identifier(getElementVariableName(element_id_function->elementVariableReference())));
    return GQLExpr::functionCall("ELEMENT_ID", std::move(arguments));
  }

  if (auto *case_expr = context->caseExpression()) {
    return makeCaseExpression(case_expr, *this);
  }

  if (auto *cast_spec = context->castSpecification()) {
    return makeCastSpecification(cast_spec, *this);
  }

  if (auto structured = tryMakeStructuredValuePrimary(context, *this)) return structured;

  throwUnsupported("value expression primary", context);
}

std::any GQLParseTreeVisitor::visitUnsignedValueSpecification(GQLParser::UnsignedValueSpecificationContext *context) {
  if (context->unsignedLiteral()) {
    return castAny<Ptr>(visit(context->unsignedLiteral()));
  }

  if (auto *general = context->generalValueSpecification()) {
    if (general->dynamicParameterSpecification()) {
      return GQLExpr::dynamicParameter(getText(general->dynamicParameterSpecification()));
    }

    if (general->SESSION_USER()) {
      return GQLExpr::specialValue("SESSION_USER");
    }
  }

  throwUnsupported("unsigned value specification", context);
}

std::any GQLParseTreeVisitor::visitUnsignedLiteral(GQLParser::UnsignedLiteralContext *context) {
  if (auto *general_literal = context->generalLiteral()) {
    if (general_literal->BOOLEAN_LITERAL()) {
      return GQLExpr::specialValue(general_literal->BOOLEAN_LITERAL()->getText());
    }

    if (general_literal->nullLiteral()) {
      return GQLExpr::specialValue("NULL");
    }

    if (auto *temporal = general_literal->temporalLiteral()) {
      if (auto *date_lit = temporal->dateLiteral())
        return GQLExpr::temporalLiteral("DATE", GQLExpr::literal(getText(date_lit->dateString())));
      if (auto *time_lit = temporal->timeLiteral())
        return GQLExpr::temporalLiteral("TIME", GQLExpr::literal(getText(time_lit->timeString())));
      if (auto *dt_lit = temporal->datetimeLiteral()) {
        String keyword = dt_lit->TIMESTAMP() ? "TIMESTAMP" : "DATETIME";
        return GQLExpr::temporalLiteral(keyword, GQLExpr::literal(getText(dt_lit->datetimeString())));
      }
    }

    if (auto *dur = general_literal->durationLiteral()) return GQLExpr::durationLiteral(GQLExpr::literal(getText(dur->durationString())));

    if (auto *list_literal = general_literal->listLiteral()) {
      return makeListConstructor(list_literal->listValueConstructorByEnumeration(), *this);
    }

    if (auto *record_literal = general_literal->recordLiteral()) {
      return makeRecordConstructor(record_literal->recordConstructor(), *this);
    }
  }

  return GQLExpr::literal(getText(context));
}

std::any GQLParseTreeVisitor::visitLinearDataModifyingStatement(GQLParser::LinearDataModifyingStatementContext *context) {
  return makeLinearDataModifyingQuery(context, *this);
}

}  // namespace OPENGQL

}  // namespace DB
