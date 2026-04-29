#include <Parsers/graph/visitor/GQLVisitorCommon.h>

namespace DB {

namespace OPENGQL {

namespace {

void replaceAll(String &text, std::string_view from, std::string_view to) {
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != String::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

String normalizeTypeLeaf(String text) {
  replaceAll(text, "NOTNULL", " NOT NULL");
  replaceAll(text, "DOUBLEPRECISION", "DOUBLE PRECISION");
  replaceAll(text, "SMALLINTEGER", "SMALL INTEGER");
  replaceAll(text, "BIGINTEGER", "BIG INTEGER");
  replaceAll(text, "YEARTOMONTH", "YEAR TO MONTH");
  replaceAll(text, "DAYTOSECOND", "DAY TO SECOND");
  replaceAll(text, "WITHTIMEZONE", "WITH TIME ZONE");
  replaceAll(text, "WITHOUTTIMEZONE", "WITHOUT TIME ZONE");
  return text;
}

void applyPrefix(Ptr &type, GQLParser::TypedContext *typed_context) {
  if (!type || !typed_context) return;

  auto *typed = type->as<GQLTypeExpression>();
  if (!typed) return;

  typed->prefix = typed_context->DOUBLE_COLON() ? GQLTypeExpression::Prefix::DoubleColon : GQLTypeExpression::Prefix::Typed;
}

Ptr makeTypeName(antlr4::ParserRuleContext *context) { return GQLTypeExpression::nameType(normalizeTypeLeaf(getText(context))); }

Ptr makeValueTypeWithoutPrefix(GQLParser::ValueTypeContext *context);
Ptr makeFieldTypes(GQLParser::FieldTypesSpecificationContext *context);
Ptr makePropertyTypes(GQLParser::PropertyTypesSpecificationContext *context);
Ptr makeElementNodeFromSource(GQLParser::SourceNodeTypeReferenceContext *context);
Ptr makeElementNodeFromDestination(GQLParser::DestinationNodeTypeReferenceContext *context);

std::vector<String> makeLabelSet(GQLParser::LabelSetSpecificationContext *context) {
  std::vector<String> labels;
  if (!context) return labels;

  for (auto *label : context->labelName()) labels.push_back(getText(label));

  return labels;
}

std::vector<String> makeLabelSet(GQLParser::LabelSetPhraseContext *context) {
  if (!context) return {};

  if (context->labelName()) return {getText(context->labelName())};

  return makeLabelSet(context->labelSetSpecification());
}

Ptr makeField(String name, GQLParser::TypedContext *typed_context, GQLParser::ValueTypeContext *value_type) {
  auto field = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::Field, std::move(name));
  auto type = makeValueType(typed_context, value_type);
  if (type) field->children.push_back(std::move(type));
  return Ptr(field);
}

Ptr makeFieldTypes(GQLParser::FieldTypesSpecificationContext *context) {
  auto fields = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::FieldList);
  if (!context || !context->fieldTypeList()) return Ptr(fields);

  for (auto *field_type : context->fieldTypeList()->fieldType())
    fields->children.push_back(makeField(getText(field_type->fieldName()), field_type->typed(), field_type->valueType()));

  return Ptr(fields);
}

Ptr makePropertyTypes(GQLParser::PropertyTypesSpecificationContext *context) {
  auto fields = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::FieldList);
  if (!context || !context->propertyTypeList()) return Ptr(fields);

  for (auto *property_type : context->propertyTypeList()->propertyType()) {
    auto *property_value_type = property_type->propertyValueType();
    fields->children.push_back(makeField(getText(property_type->propertyName()), property_type->typed(),
                                         property_value_type ? property_value_type->valueType() : nullptr));
  }

  return Ptr(fields);
}

Ptr makeRecordType(GQLParser::RecordTypeContext *context) {
  auto record = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::Record);
  record->any_keyword = context && context->ANY();
  record->not_null = context && context->notNull();

  if (context && context->fieldTypesSpecification()) {
    auto fields = makeFieldTypes(context->fieldTypesSpecification());
    if (fields) {
      for (auto &child : fields->children) record->children.push_back(child);
    }
  }

  return Ptr(record);
}

Ptr makeListType(String name, Ptr element_type, String length, bool not_null) {
  auto list = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::List, std::move(name));
  list->length = std::move(length);
  list->not_null = not_null;
  if (element_type) list->children.push_back(std::move(element_type));
  return Ptr(list);
}

Ptr makeReferenceValueType(GQLParser::ReferenceValueTypeContext *context) {
  if (context->graphReferenceValueType()) return makeGraphReferenceValueType(nullptr, context->graphReferenceValueType());

  if (context->bindingTableReferenceValueType())
    return makeBindingTableReferenceValueType(nullptr, context->bindingTableReferenceValueType());

  if (auto *node = context->nodeReferenceValueType()) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::NodeReference);
    if (auto *open = node->openNodeReferenceValueType()) {
      result->any_keyword = open->ANY() != nullptr;
      result->name = getText(open->nodeSynonym());
      result->not_null = open->notNull() != nullptr;
    } else if (auto *closed = node->closedNodeReferenceValueType()) {
      result->not_null = closed->notNull() != nullptr;
      auto element = makeNodeTypeSpecification(closed->nodeTypeSpecification());
      if (element) result->children.push_back(std::move(element));
    }
    return Ptr(result);
  }

  if (auto *edge = context->edgeReferenceValueType()) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::EdgeReference);
    if (auto *open = edge->openEdgeReferenceValueType()) {
      result->any_keyword = open->ANY() != nullptr;
      result->name = getText(open->edgeSynonym());
      result->not_null = open->notNull() != nullptr;
    } else if (auto *closed = edge->closedEdgeReferenceValueType()) {
      result->not_null = closed->notNull() != nullptr;
      auto element = makeEdgeTypeSpecification(closed->edgeTypeSpecification());
      if (element) result->children.push_back(std::move(element));
    }
    return Ptr(result);
  }

  return makeTypeName(context);
}

Ptr makePredefinedType(GQLParser::PredefinedTypeContext *context) {
  if (context->referenceValueType()) return makeReferenceValueType(context->referenceValueType());

  return makeTypeName(context);
}

Ptr makeValueTypeWithoutPrefix(GQLParser::ValueTypeContext *context) {
  if (!context) return {};

  if (auto *predefined = dynamic_cast<GQLParser::PredefinedTypeLabelContext *>(context))
    return makePredefinedType(predefined->predefinedType());

  if (auto *path = dynamic_cast<GQLParser::PathValueTypeLabelContext *>(context)) return makeTypeName(path->pathValueType());

  if (auto *record = dynamic_cast<GQLParser::RecordTypeLabelContext *>(context)) return makeRecordType(record->recordType());

  if (auto *list = dynamic_cast<GQLParser::ListValueTypeAlt1Context *>(context))
    return makeListType(getText(list->listValueTypeName()), makeValueTypeWithoutPrefix(list->valueType()), getText(list->maxLength()),
                        list->notNull() != nullptr);

  if (auto *list = dynamic_cast<GQLParser::ListValueTypeAlt2Context *>(context))
    return makeListType(getText(list->listValueTypeName()), makeValueTypeWithoutPrefix(list->valueType()), getText(list->maxLength()),
                        list->notNull() != nullptr);

  if (auto *list = dynamic_cast<GQLParser::ListValueTypeAlt3Context *>(context))
    return makeListType(getText(list->listValueTypeName()), {}, getText(list->maxLength()), list->notNull() != nullptr);

  if (auto *open_dynamic = dynamic_cast<GQLParser::OpenDynamicUnionTypeLabelContext *>(context)) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::DynamicAny);
    result->value_keyword = open_dynamic->VALUE() != nullptr;
    result->not_null = open_dynamic->notNull() != nullptr;
    return Ptr(result);
  }

  if (auto *dynamic_property = dynamic_cast<GQLParser::DynamicPropertyValueTypeLabelContext *>(context)) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::DynamicProperty);
    result->any_keyword = dynamic_property->ANY() != nullptr;
    result->not_null = dynamic_property->notNull() != nullptr;
    return Ptr(result);
  }

  if (auto *closed_dynamic = dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl1Context *>(context)) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::Union, "ANY VALUE");
    for (auto *type : closed_dynamic->valueType()) result->children.push_back(makeValueTypeWithoutPrefix(type));
    return Ptr(result);
  }

  if (auto *union_type = dynamic_cast<GQLParser::ClosedDynamicUnionTypeAtl2Context *>(context)) {
    auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::Union);
    for (auto *type : union_type->valueType()) result->children.push_back(makeValueTypeWithoutPrefix(type));
    return Ptr(result);
  }

  return makeTypeName(context);
}

void populateNodeFiller(GQLElementTypeSpecification &node, GQLParser::NodeTypeFillerContext *filler) {
  if (!filler) return;

  if (auto *key = filler->nodeTypeKeyLabelSet()) {
    auto labels = makeLabelSet(key->labelSetPhrase());
    node.labels.insert(node.labels.end(), labels.begin(), labels.end());
  }

  auto *implied = filler->nodeTypeImpliedContent();
  if (!implied) return;

  if (auto *label_set = implied->nodeTypeLabelSet()) {
    auto labels = makeLabelSet(label_set->labelSetPhrase());
    node.labels.insert(node.labels.end(), labels.begin(), labels.end());
  }

  if (auto *property_types = implied->nodeTypePropertyTypes()) {
    node.properties = makePropertyTypes(property_types->propertyTypesSpecification());
    if (node.properties) node.children.push_back(node.properties);
  }
}

void populateEdgeFiller(GQLElementTypeSpecification &edge, GQLParser::EdgeTypeFillerContext *filler) {
  if (!filler) return;

  if (auto *key = filler->edgeTypeKeyLabelSet()) {
    auto labels = makeLabelSet(key->labelSetPhrase());
    edge.labels.insert(edge.labels.end(), labels.begin(), labels.end());
  }

  auto *implied = filler->edgeTypeImpliedContent();
  if (!implied) return;

  if (auto *label_set = implied->edgeTypeLabelSet()) {
    auto labels = makeLabelSet(label_set->labelSetPhrase());
    edge.labels.insert(edge.labels.end(), labels.begin(), labels.end());
  }

  if (auto *property_types = implied->edgeTypePropertyTypes()) {
    edge.properties = makePropertyTypes(property_types->propertyTypesSpecification());
    if (edge.properties) edge.children.push_back(edge.properties);
  }
}

Ptr makeElementNodeFromSource(GQLParser::SourceNodeTypeReferenceContext *context) {
  auto node = make_intrusive<GQLElementTypeSpecification>(GQLElementTypeSpecification::Kind::Node);
  if (!context) return Ptr(node);

  if (context->sourceNodeTypeAlias()) node->alias = getText(context->sourceNodeTypeAlias());
  populateNodeFiller(*node, context->nodeTypeFiller());
  return Ptr(node);
}

Ptr makeElementNodeFromDestination(GQLParser::DestinationNodeTypeReferenceContext *context) {
  auto node = make_intrusive<GQLElementTypeSpecification>(GQLElementTypeSpecification::Kind::Node);
  if (!context) return Ptr(node);

  if (context->destinationNodeTypeAlias()) node->alias = getText(context->destinationNodeTypeAlias());
  populateNodeFiller(*node, context->nodeTypeFiller());
  return Ptr(node);
}

}  // namespace

Ptr makeValueType(GQLParser::TypedContext *typed_context, GQLParser::ValueTypeContext *type_context) {
  auto type = makeValueTypeWithoutPrefix(type_context);
  applyPrefix(type, typed_context);
  return type;
}

Ptr makeGraphReferenceValueType(GQLParser::TypedContext *typed_context, GQLParser::GraphReferenceValueTypeContext *type_context) {
  if (!type_context) return {};

  auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::GraphReference);

  if (auto *open = type_context->openGraphReferenceValueType()) {
    result->any_keyword = true;
    result->property_keyword = open->PROPERTY() != nullptr;
    result->not_null = open->notNull() != nullptr;
  } else if (auto *closed = type_context->closedGraphReferenceValueType()) {
    result->property_keyword = closed->PROPERTY() != nullptr;
    result->not_null = closed->notNull() != nullptr;
    auto graph_type = makeNestedGraphTypeSpecification(closed->nestedGraphTypeSpecification());
    if (graph_type) result->children.push_back(std::move(graph_type));
  }

  Ptr type = Ptr(result);
  applyPrefix(type, typed_context);
  return type;
}

Ptr makeBindingTableReferenceValueType(GQLParser::TypedContext *typed_context,
                                       GQLParser::BindingTableReferenceValueTypeContext *type_context) {
  if (!type_context) return {};

  auto result = make_intrusive<GQLTypeExpression>(GQLTypeExpression::Kind::BindingTableReference);
  auto *binding_table_type = type_context->bindingTableType();
  result->binding_keyword = binding_table_type && binding_table_type->BINDING();
  result->not_null = type_context->notNull() != nullptr;
  if (binding_table_type && binding_table_type->fieldTypesSpecification()) {
    auto fields = makeFieldTypes(binding_table_type->fieldTypesSpecification());
    if (fields) {
      for (auto &child : fields->children) result->children.push_back(child);
    }
  }

  Ptr type = Ptr(result);
  applyPrefix(type, typed_context);
  return type;
}

Ptr makeNestedGraphTypeSpecification(GQLParser::NestedGraphTypeSpecificationContext *context) {
  auto graph_type = make_intrusive<GQLGraphTypeSpecification>();
  if (!context || !context->graphTypeSpecificationBody() || !context->graphTypeSpecificationBody()->elementTypeList())
    return Ptr(graph_type);

  for (auto *element : context->graphTypeSpecificationBody()->elementTypeList()->elementTypeSpecification()) {
    if (element->nodeTypeSpecification())
      graph_type->children.push_back(makeNodeTypeSpecification(element->nodeTypeSpecification()));
    else if (element->edgeTypeSpecification())
      graph_type->children.push_back(makeEdgeTypeSpecification(element->edgeTypeSpecification()));
  }

  return Ptr(graph_type);
}

Ptr makeNodeTypeSpecification(GQLParser::NodeTypeSpecificationContext *context) {
  auto node = make_intrusive<GQLElementTypeSpecification>(GQLElementTypeSpecification::Kind::Node);
  if (!context) return Ptr(node);

  if (auto *pattern = context->nodeTypePattern()) {
    node->syntax = GQLElementTypeSpecification::Syntax::Pattern;
    if (pattern->nodeTypeName()) node->type_name = getText(pattern->nodeTypeName());
    if (pattern->localNodeTypeAlias()) node->alias = getText(pattern->localNodeTypeAlias());
    populateNodeFiller(*node, pattern->nodeTypeFiller());
    return Ptr(node);
  }

  if (auto *phrase = context->nodeTypePhrase()) {
    node->syntax = GQLElementTypeSpecification::Syntax::Phrase;
    if (auto *filler = phrase->nodeTypePhraseFiller()) {
      if (filler->nodeTypeName()) node->type_name = getText(filler->nodeTypeName());
      populateNodeFiller(*node, filler->nodeTypeFiller());
    }
    if (phrase->localNodeTypeAlias()) node->alias = getText(phrase->localNodeTypeAlias());
  }

  return Ptr(node);
}

Ptr makeEdgeTypeSpecification(GQLParser::EdgeTypeSpecificationContext *context) {
  auto edge = make_intrusive<GQLElementTypeSpecification>(GQLElementTypeSpecification::Kind::Edge);
  if (!context) return Ptr(edge);

  auto attach_endpoint = [&edge](Ptr source, Ptr destination) {
    edge->source = std::move(source);
    edge->destination = std::move(destination);
    edge->children.clear();
    if (edge->source) edge->children.push_back(edge->source);
    if (edge->properties) edge->children.push_back(edge->properties);
    if (edge->destination) edge->children.push_back(edge->destination);
  };

  if (auto *pattern = context->edgeTypePattern()) {
    edge->syntax = GQLElementTypeSpecification::Syntax::Pattern;
    if (pattern->edgeTypeName()) edge->type_name = getText(pattern->edgeTypeName());

    if (auto *directed = pattern->edgeTypePatternDirected()) {
      if (auto *right = directed->edgeTypePatternPointingRight()) {
        edge->direction = EdgeDirection::Right;
        populateEdgeFiller(*edge, right->arcTypePointingRight()->edgeTypeFiller());
        attach_endpoint(makeElementNodeFromSource(right->sourceNodeTypeReference()),
                        makeElementNodeFromDestination(right->destinationNodeTypeReference()));
      } else if (auto *left = directed->edgeTypePatternPointingLeft()) {
        edge->direction = EdgeDirection::Left;
        populateEdgeFiller(*edge, left->arcTypePointingLeft()->edgeTypeFiller());
        attach_endpoint(makeElementNodeFromSource(left->sourceNodeTypeReference()),
                        makeElementNodeFromDestination(left->destinationNodeTypeReference()));
      }
    } else if (auto *undirected = pattern->edgeTypePatternUndirected()) {
      edge->direction = EdgeDirection::Undirected;
      populateEdgeFiller(*edge, undirected->arcTypeUndirected()->edgeTypeFiller());
      attach_endpoint(makeElementNodeFromSource(undirected->sourceNodeTypeReference()),
                      makeElementNodeFromDestination(undirected->destinationNodeTypeReference()));
    }

    return Ptr(edge);
  }

  edge->syntax = GQLElementTypeSpecification::Syntax::Phrase;
  return Ptr(edge);
}

}  // namespace OPENGQL

}  // namespace DB
