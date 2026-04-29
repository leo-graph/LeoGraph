#pragma once

#include <Parsers/graph/AST/Utils.h>

#include <vector>

namespace DB::OPENGQL::AST {

class GQLTypeExpression final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Name,
    List,
    Record,
    FieldList,
    Field,
    Union,
    DynamicAny,
    DynamicProperty,
    GraphReference,
    BindingTableReference,
    NodeReference,
    EdgeReference,
  };

  enum class Prefix : UInt8 {
    None,
    Typed,
    DoubleColon,
  };

  explicit GQLTypeExpression(Kind kind_, String name_ = {}) : kind(kind_), name(std::move(name_)) {}

  static Ptr nameType(String name) { return Ptr(make_intrusive<GQLTypeExpression>(Kind::Name, std::move(name))); }

  String getID(char delim) const override { return "GQLTypeExpression" + (delim + name); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLTypeExpression>(*this);
    result->children.clear();

    for (const auto& child : children) {
      if (child) result->children.push_back(child->clone());
    }

    return result;
  }

  Kind kind;
  Prefix prefix = Prefix::None;
  String name;
  String length;
  std::vector<String> parameters;
  bool not_null = false;
  bool any_keyword = false;
  bool value_keyword = false;
  bool property_keyword = false;
  bool binding_keyword = false;

 protected:
  void formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const override {
    formatPrefix(ostr);

    switch (kind) {
      case Kind::Name:
        formatName(ostr);
        break;

      case Kind::List:
        formatList(ostr, settings, state, frame);
        break;

      case Kind::Record:
        formatRecord(ostr, settings, state, frame);
        break;

      case Kind::FieldList:
        formatFieldList(ostr, settings, state, frame);
        break;

      case Kind::Field:
        ostr << name;
        if (!children.empty() && children.front()) {
          ostr << " ";
          children.front()->format(ostr, settings, state, frame);
        }
        break;

      case Kind::Union:
        formatUnion(ostr, settings, state, frame);
        break;

      case Kind::DynamicAny:
        ostr << "ANY";
        if (value_keyword) ostr << " VALUE";
        break;

      case Kind::DynamicProperty:
        if (any_keyword) ostr << "ANY ";
        ostr << "PROPERTY VALUE";
        break;

      case Kind::GraphReference:
        formatGraphReference(ostr, settings, state, frame);
        break;

      case Kind::BindingTableReference:
        formatBindingTableReference(ostr, settings, state, frame);
        break;

      case Kind::NodeReference:
      case Kind::EdgeReference:
        formatElementReference(ostr, settings, state, frame);
        break;
    }

    if (not_null) ostr << " NOT NULL";
  }

 private:
  void formatPrefix(WriteBuffer& ostr) const {
    if (prefix == Prefix::Typed)
      ostr << "TYPED ";
    else if (prefix == Prefix::DoubleColon)
      ostr << ":: ";
  }

  void formatName(WriteBuffer& ostr) const {
    ostr << name;
    if (!parameters.empty()) {
      ostr << "(";
      for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) ostr << ", ";
        ostr << parameters[i];
      }
      ostr << ")";
    }
  }

  void formatList(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    ostr << (name.empty() ? "LIST" : name);
    if (!children.empty() && children.front()) {
      ostr << "<";
      children.front()->format(ostr, settings, state, frame);
      ostr << ">";
    }
    if (!length.empty()) ostr << "[" << length << "]";
  }

  void formatRecord(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    if (any_keyword) ostr << "ANY ";

    ostr << "RECORD";
    if (!children.empty()) {
      ostr << " ";
      formatFieldList(ostr, settings, state, frame);
    }
  }

  void formatFieldList(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    ostr << "{";
    detail::formatChildren(ostr, settings, state, frame, children, ", ");
    ostr << "}";
  }

  void formatUnion(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    if (!name.empty()) ostr << name << "<";

    detail::formatChildren(ostr, settings, state, frame, children, " | ");

    if (!name.empty()) ostr << ">";
  }

  void formatGraphReference(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    if (any_keyword) ostr << "ANY ";
    if (property_keyword) ostr << "PROPERTY ";

    ostr << "GRAPH";
    if (!children.empty() && children.front()) {
      ostr << " ";
      children.front()->format(ostr, settings, state, frame);
    }
  }

  void formatBindingTableReference(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    if (binding_keyword) ostr << "BINDING ";

    ostr << "TABLE";
    if (!children.empty()) {
      ostr << " ";
      formatFieldList(ostr, settings, state, frame);
    }
  }

  void formatElementReference(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
    if (!children.empty() && children.front()) {
      children.front()->format(ostr, settings, state, frame);
      return;
    }

    if (any_keyword) ostr << "ANY ";

    ostr << (name.empty() ? (kind == Kind::NodeReference ? "NODE" : "EDGE") : name);
  }
};

}  // namespace DB::OPENGQL::AST
