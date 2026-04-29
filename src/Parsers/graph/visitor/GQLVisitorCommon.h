#pragma once
#include <Common/Exception.h>
#include <Parsers/graph/visitor/GQLParseTreeVisitor.h>

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

using namespace AST;

inline String getText(antlr4::ParserRuleContext *context) { return context ? context->getText() : String{}; }

template <typename T>
T castAny(const std::any &value) {
  return std::any_cast<T>(value);
}

[[noreturn]] inline void throwUnsupported(const String &feature, antlr4::ParserRuleContext *context) {
  throw Exception(ErrorCodes::SYNTAX_ERROR, "Unsupported GQL {} in the current graph AST visitor: {}", feature, getText(context));
}

inline Ptr makeAliasedItem(Ptr expression, String alias = {}) {
  return Ptr(make_intrusive<GQLAliasedItem>(std::move(expression), std::move(alias)));
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

Ptr makeGraphPatternBlock(PatternBindingTable &&binding_table, bool parenthesized);
void populateMatchStatementBlock(GQLMatchStatementBlock &block, GQLParseTreeVisitor &visitor,
                                 GQLParser::MatchStatementBlockContext *context);
Ptr makeNodeOrEdgePattern(ElementPatternParts &&parts, bool is_edge, EdgeDirection direction = EdgeDirection::Right);
Ptr makeLinearDataModifyingQuery(GQLParser::LinearDataModifyingStatementContext *context, GQLParseTreeVisitor &visitor);

struct ResultTail {
  Ptr order_by;
  Ptr offset;
  Ptr limit;
};

Ptr makeOrderByAndPageClause(ResultTail &&tail);

inline CombinedQueryOperator getCombinedQueryOperator(GQLParser::CompositeQueryExpressionContext *context) {
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

inline void appendCombinedQueryPart(Ptr query, PtrList &queries, std::vector<CombinedQueryOperator> &operators) {
  if (!query) return;

  if (const auto *combined_query = query->as<GQLCombinedQuery>()) {
    for (const auto &child_query : combined_query->queries) queries.push_back(child_query);

    for (const auto operation : combined_query->operators) operators.push_back(operation);

    return;
  }

  queries.push_back(std::move(query));
}

inline Ptr makeCallVariableScopeClause(GQLParser::VariableScopeClauseContext *context) {
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

inline void appendClause(PtrList &clauses, Ptr clause) {
  if (clause) clauses.push_back(std::move(clause));
}

inline String getElementVariableName(GQLParser::ElementVariableReferenceContext *context) {
  auto *binding_reference = context ? context->bindingVariableReference() : nullptr;
  auto *binding_variable = binding_reference ? binding_reference->bindingVariable() : nullptr;
  return getText(binding_variable);
}

inline String getTypedRawType(GQLParser::TypedContext *typed_context, antlr4::ParserRuleContext *type_context) {
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

inline std::vector<String> getDirectoryParts(GQLParser::SimpleDirectoryPathContext *context) {
  std::vector<String> parts;

  if (!context) {
    return parts;
  }

  for (auto *directory_name : context->directoryName()) {
    parts.push_back(getText(directory_name));
  }

  return parts;
}

inline Ptr makeSchemaReference(GQLParser::SchemaReferenceContext *context) {
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

inline void populateParentReference(GQLCatalogObjectName &obj, GQLParser::CatalogObjectParentReferenceContext *parent_ref) {
  if (!parent_ref) return;

  if (auto *schema = parent_ref->schemaReference()) {
    obj.schema_ref = makeSchemaReference(schema);
    if (obj.schema_ref) obj.children.push_back(obj.schema_ref);
    obj.has_slash_after_schema = (parent_ref->SOLIDUS() != nullptr);
  }

  for (auto *obj_name : parent_ref->objectName()) obj.parent_parts.push_back(getText(obj_name));
}

inline Ptr makeCatalogObjectName(String name, GQLParser::CatalogObjectParentReferenceContext *parent_ref) {
  auto obj_name = make_intrusive<GQLCatalogObjectName>(std::move(name));
  populateParentReference(*obj_name, parent_ref);
  return Ptr(obj_name);
}

inline Ptr makeCatalogObjectName(GQLParser::CatalogGraphParentAndNameContext *context) {
  return makeCatalogObjectName(getText(context->graphName()), context->catalogObjectParentReference());
}

inline Ptr makeCatalogObjectName(GQLParser::CatalogGraphTypeParentAndNameContext *context) {
  return makeCatalogObjectName(getText(context->graphTypeName()), context->catalogObjectParentReference());
}

inline Ptr makeCatalogObjectName(GQLParser::CatalogProcedureParentAndNameContext *context) {
  return makeCatalogObjectName(getText(context->procedureName()), context->catalogObjectParentReference());
}

inline Ptr makeCatalogObjectName(GQLParser::BindingTableReferenceContext *context) {
  return makeCatalogObjectName(getText(context->bindingTableName()), context->catalogObjectParentReference());
}

inline Ptr makeProcedureReference(GQLParser::ProcedureReferenceContext *context) {
  if (auto *name = context->catalogProcedureParentAndName()) return makeCatalogObjectName(name);

  if (auto *parameter = context->referenceParameterSpecification()) return GQLExpr::literal(getText(parameter));

  throwUnsupported("procedure reference", context);
}

inline Ptr makeBindingInitializer(GQLBindingInitializer::Kind kind, Ptr value) {
  return Ptr(make_intrusive<GQLBindingInitializer>(kind, std::move(value)));
}

Ptr makeObjectExpressionPrimary(GQLParser::ObjectExpressionPrimaryContext *context, GQLParseTreeVisitor &visitor);

inline Ptr makeGraphExpression(GQLParser::GraphExpressionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) {
    return {};
  }

  if (auto *graph_ref = context->graphReference()) {
    if (graph_ref->catalogObjectParentReference() && graph_ref->graphName()) {
      return Ptr(make_intrusive<GQLGraphExpression>(
          GQLGraphExpression::Kind::QualifiedGraphRef, String{},
          makeCatalogObjectName(getText(graph_ref->graphName()), graph_ref->catalogObjectParentReference())));
    }
    if (graph_ref->homeGraph())
      return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::HomeGraphRef, getText(graph_ref->homeGraph())));
    if (graph_ref->referenceParameterSpecification())
      return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::ParameterGraphRef,
                                                    getText(graph_ref->referenceParameterSpecification())));
    return Ptr(make_intrusive<GQLGraphExpression>(GQLGraphExpression::Kind::DelimitedGraphRef, getText(graph_ref)));
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
                                                  makeObjectExpressionPrimary(context->objectExpressionPrimary(), visitor)));
  }

  throwUnsupported("graph expression", context);
}

inline Ptr makeBindingTableExpression(GQLParser::BindingTableExpressionContext *context, GQLParseTreeVisitor &visitor) {
  if (!context) {
    return {};
  }

  if (context->nestedBindingTableQuerySpecification()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(
        GQLBindingTableExpression::Kind::NestedQuery, String{},
        castAny<Ptr>(visitor.visit(context->nestedBindingTableQuerySpecification()->nestedQuerySpecification()))));
  }

  if (context->bindingTableReference()) {
    auto *reference = context->bindingTableReference();
    if (reference->catalogObjectParentReference() && reference->bindingTableName()) {
      return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::BindingTableReference, String{},
                                                           makeCatalogObjectName(reference)));
    }

    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::BindingTableReference, getText(reference)));
  }

  if (context->objectNameOrBindingVariable()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::ObjectNameOrBindingVariable,
                                                         getText(context->objectNameOrBindingVariable())));
  }

  if (context->objectExpressionPrimary()) {
    return Ptr(make_intrusive<GQLBindingTableExpression>(GQLBindingTableExpression::Kind::ObjectExpression, String{},
                                                         makeObjectExpressionPrimary(context->objectExpressionPrimary(), visitor)));
  }

  throwUnsupported("binding table expression", context);
}

inline PtrList makeYieldItems(GQLParser::YieldItemListContext *context) {
  PtrList items;

  if (!context) {
    return items;
  }

  for (auto *item : context->yieldItem()) {
    auto expression = GQLExpr::identifier(getText(item->yieldItemName()->fieldName()));
    String alias;

    if (item->yieldItemAlias()) alias = getText(item->yieldItemAlias()->bindingVariable());

    items.push_back(makeAliasedItem(expression, std::move(alias)));
  }

  return items;
}

inline Ptr makeYieldClause(GQLParser::YieldClauseContext *context) {
  auto clause = make_intrusive<GQLYieldClause>();
  clause->items = makeYieldItems(context ? context->yieldItemList() : nullptr);

  for (auto &item : clause->items) {
    appendClause(clause->children, item);
  }

  return Ptr(clause);
}

inline Ptr makeUseGraphClause(GQLParser::UseGraphClauseContext *context, GQLParseTreeVisitor &visitor) {
  return Ptr(make_intrusive<GQLUseClause>(makeGraphExpression(context ? context->graphExpression() : nullptr, visitor)));
}

inline Ptr makeSingleQuery(PtrList clauses) { return Ptr(make_intrusive<GQLSingleQuery>(std::move(clauses))); }

inline void appendClauseList(PtrList &clauses, PtrList &&extra_clauses) {
  for (auto &clause : extra_clauses) appendClause(clauses, std::move(clause));
}

inline void appendQueryResultClause(PtrList &clauses, Ptr clause) {
  if (!clause) return;

  if (const auto *single_query = clause->as<GQLSingleQuery>()) {
    for (const auto &nested_clause : single_query->clauses) clauses.push_back(nested_clause);

    return;
  }

  clauses.push_back(std::move(clause));
}

inline Ptr makeCallClause(GQLParser::CallProcedureStatementContext *statement, GQLParseTreeVisitor &visitor) {
  auto *procedure_call = statement->procedureCall();

  if (auto *named = procedure_call->namedProcedureCall()) {
    auto clause = make_intrusive<GQLCallNamedClause>();
    clause->optional = statement->OPTIONAL() != nullptr;
    clause->procedure = makeProcedureReference(named->procedureReference());

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

}  // namespace OPENGQL

}  // namespace DB
