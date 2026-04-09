#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLListConstructor final : public DB::IAST {
 public:
  PtrList items;

  String getID(char) const override { return "GQLListConstructor"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLListConstructor>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "[";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
    ostr << "]";
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
