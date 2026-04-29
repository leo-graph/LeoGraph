#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLSetItem final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Property,       // n.prop = expr
    AllProperties,  // n = { key: val, ... }
    Label,          // n IS label / n:label
  };

  GQLSetItem(Kind kind_, String variable_, String property_or_label_ = {})
      : kind(kind_), variable(std::move(variable_)), property_or_label(std::move(property_or_label_)) {}

  String getID(char delim) const override { return "GQLSetItem" + (delim + variable); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLSetItem>(*this);
    result->children.clear();
    result->value = value ? value->clone() : Ptr{};
    if (result->value) result->children.push_back(result->value);
    return result;
  }

  Kind kind;
  String variable;
  String property_or_label;
  bool use_is = false;
  Ptr value;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << variable;

    switch (kind) {
      case Kind::Property:
        ostr << "." << property_or_label << " = ";
        if (value) value->format(ostr, settings, state, frame);
        break;

      case Kind::AllProperties:
        ostr << " = ";
        if (value) value->format(ostr, settings, state, frame);
        break;

      case Kind::Label:
        ostr << (use_is ? " IS " : ":") << property_or_label;
        break;
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &value); }
};

class GQLSetClause final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLSetClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLSetClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

  PtrList items;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "SET ";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
