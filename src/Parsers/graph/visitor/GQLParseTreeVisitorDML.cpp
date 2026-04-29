#include <Parsers/graph/visitor/GQLVisitorCommon.h>

namespace DB {

namespace OPENGQL {

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

std::any GQLParseTreeVisitor::visitLinearDataModifyingStatement(GQLParser::LinearDataModifyingStatementContext *context) {
  return makeLinearDataModifyingQuery(context, *this);
}

}  // namespace OPENGQL

}  // namespace DB
