#include <Parsers/graph/visitor/GQLVisitorCommon.h>

namespace DB {

namespace OPENGQL {

Ptr makeSchemaReferenceFromCatalogName(GQLParser::CatalogSchemaParentAndNameContext *context) {
  auto reference = make_intrusive<GQLSchemaReference>(GQLSchemaReference::Kind::AbsolutePath, getText(context->schemaName()));
  if (auto *abs_dir = context->absoluteDirectoryPath()) {
    reference->directory_parts = getDirectoryParts(abs_dir->simpleDirectoryPath());
  }
  return Ptr(reference);
}

Ptr makeGraphTypeRef(GQLParser::GraphTypeReferenceContext *context) {
  if (auto *name = context->catalogGraphTypeParentAndName()) return makeCatalogObjectName(name);
  if (auto *param = context->referenceParameterSpecification()) return GQLExpr::literal(getText(param));
  return GQLExpr::literal(getText(context));
}

std::any GQLParseTreeVisitor::visitLinearCatalogModifyingStatement(GQLParser::LinearCatalogModifyingStatementContext *context) {
  PtrList stmts;
  for (auto *simple : context->simpleCatalogModifyingStatement()) {
    if (simple->primitiveCatalogModifyingStatement()) {
      stmts.push_back(castAny<Ptr>(visit(simple->primitiveCatalogModifyingStatement())));
    } else if (simple->callCatalogModifyingProcedureStatement()) {
      stmts.push_back(makeCallClause(simple->callCatalogModifyingProcedureStatement()->callProcedureStatement(), *this));
    }
  }
  return makeSingleQuery(std::move(stmts));
}

std::any GQLParseTreeVisitor::visitCreateSchemaStatement(GQLParser::CreateSchemaStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::CreateSchema);
  stmt->if_not_exists = (context->IF() != nullptr);
  stmt->name_reference = makeSchemaReferenceFromCatalogName(context->catalogSchemaParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);
  return Ptr(stmt);
}

std::any GQLParseTreeVisitor::visitDropSchemaStatement(GQLParser::DropSchemaStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::DropSchema);
  stmt->if_exists = (context->IF() != nullptr);
  stmt->name_reference = makeSchemaReferenceFromCatalogName(context->catalogSchemaParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);
  return Ptr(stmt);
}

std::any GQLParseTreeVisitor::visitCreateGraphStatement(GQLParser::CreateGraphStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::CreateGraph);
  stmt->is_property = (context->PROPERTY() != nullptr);
  stmt->or_replace = (context->REPLACE() != nullptr);
  stmt->if_not_exists = (context->IF() != nullptr && !stmt->or_replace);
  stmt->name_reference = makeCatalogObjectName(context->catalogGraphParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);

  if (context->openGraphType()) {
    stmt->source_kind = GQLCatalogStatement::SourceKind::Any;
  } else if (auto *of_type = context->ofGraphType()) {
    if (of_type->graphTypeLikeGraph()) {
      stmt->source_kind = GQLCatalogStatement::SourceKind::LikeGraph;
      stmt->source_reference = makeGraphExpression(of_type->graphTypeLikeGraph()->graphExpression(), *this);
      if (stmt->source_reference) stmt->children.push_back(stmt->source_reference);
    } else if (of_type->graphTypeReference()) {
      stmt->source_kind = GQLCatalogStatement::SourceKind::TypeReference;
      stmt->source_reference = makeGraphTypeRef(of_type->graphTypeReference());
      if (stmt->source_reference) stmt->children.push_back(stmt->source_reference);
    } else if (of_type->nestedGraphTypeSpecification()) {
      stmt->source_kind = GQLCatalogStatement::SourceKind::NestedSpec;
      stmt->source_text = getText(of_type->nestedGraphTypeSpecification());
    }
  }

  if (context->graphSource()) {
    stmt->copy_source = makeGraphExpression(context->graphSource()->graphExpression(), *this);
    if (stmt->copy_source) stmt->children.push_back(stmt->copy_source);
  }

  return Ptr(stmt);
}

std::any GQLParseTreeVisitor::visitDropGraphStatement(GQLParser::DropGraphStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::DropGraph);
  stmt->is_property = (context->PROPERTY() != nullptr);
  stmt->if_exists = (context->IF() != nullptr);
  stmt->name_reference = makeCatalogObjectName(context->catalogGraphParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);
  return Ptr(stmt);
}

std::any GQLParseTreeVisitor::visitCreateGraphTypeStatement(GQLParser::CreateGraphTypeStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::CreateGraphType);
  stmt->is_property = (context->PROPERTY() != nullptr);
  stmt->or_replace = (context->REPLACE() != nullptr);
  stmt->if_not_exists = (context->IF() != nullptr && !stmt->or_replace);
  stmt->name_reference = makeCatalogObjectName(context->catalogGraphTypeParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);

  auto *source = context->graphTypeSource();
  if (source->copyOfGraphType()) {
    stmt->source_kind = GQLCatalogStatement::SourceKind::CopyOfType;
    stmt->source_reference = makeGraphTypeRef(source->copyOfGraphType()->graphTypeReference());
    if (stmt->source_reference) stmt->children.push_back(stmt->source_reference);
  } else if (source->graphTypeLikeGraph()) {
    stmt->source_kind = GQLCatalogStatement::SourceKind::LikeGraph;
    stmt->source_reference = makeGraphExpression(source->graphTypeLikeGraph()->graphExpression(), *this);
    if (stmt->source_reference) stmt->children.push_back(stmt->source_reference);
  } else if (source->nestedGraphTypeSpecification()) {
    stmt->source_kind = GQLCatalogStatement::SourceKind::NestedSpec;
    stmt->source_text = getText(source->nestedGraphTypeSpecification());
  }

  return Ptr(stmt);
}

std::any GQLParseTreeVisitor::visitDropGraphTypeStatement(GQLParser::DropGraphTypeStatementContext *context) {
  auto stmt = make_intrusive<GQLCatalogStatement>(GQLCatalogStatement::Kind::DropGraphType);
  stmt->is_property = (context->PROPERTY() != nullptr);
  stmt->if_exists = (context->IF() != nullptr);
  stmt->name_reference = makeCatalogObjectName(context->catalogGraphTypeParentAndName());
  if (stmt->name_reference) stmt->children.push_back(stmt->name_reference);
  return Ptr(stmt);
}

}  // namespace OPENGQL

}  // namespace DB
