#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLOrderByClause final : public DB::IAST {
 public:
  GQLOrderByClause() = default;

  explicit GQLOrderByClause(PtrList items_) : items(std::move(items_)) {
    for (const auto &item : items) {
      if (item) children.push_back(item);
    }
  }

  String getID(char) const override { return "GQLOrderByClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLOrderByClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

  PtrList items;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "ORDER BY ";
    detail::formatChildren(ostr, settings, state, frame, items, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
