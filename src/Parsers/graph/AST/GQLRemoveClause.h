#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLRemoveItem final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Property,  // n.prop
    Label,     // n IS label / n:label
  };

  GQLRemoveItem(Kind kind_, String variable_, String property_or_label_, bool use_is_ = false)
      : kind(kind_), variable(std::move(variable_)), property_or_label(std::move(property_or_label_)), use_is(use_is_) {}

  String getID(char delim) const override { return "GQLRemoveItem" + (delim + variable); }

  ASTPtr clone() const override { return make_intrusive<GQLRemoveItem>(*this); }

  Kind kind;
  String variable;
  String property_or_label;
  bool use_is = false;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override {
    ostr << variable;

    switch (kind) {
      case Kind::Property:
        ostr << "." << property_or_label;
        break;

      case Kind::Label:
        ostr << (use_is ? " IS " : ":") << property_or_label;
        break;
    }
  }
};

class GQLRemoveClause final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLRemoveClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLRemoveClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

  PtrList items;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "REMOVE ";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
