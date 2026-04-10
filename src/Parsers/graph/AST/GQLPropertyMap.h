#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPropertyMap final : public DB::IAST {
 public:
  GQLPropertyMap() = default;

  explicit GQLPropertyMap(PtrList items_) : items(std::move(items_)) {
    for (const auto &item : items) {
      if (item) children.push_back(item);
    }
  }

  String getID(char) const override { return "GQLPropertyMap"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPropertyMap>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

  PtrList items;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "{";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
    ostr << "}";
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
