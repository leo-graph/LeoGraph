#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLGroupByClause final : public DB::IAST {
 public:
  PtrList items;
  bool empty_grouping_set = false;

  String getID(char) const override { return "GQLGroupByClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLGroupByClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "GROUP BY ";

    if (empty_grouping_set) {
      ostr << "()";
      return;
    }

    detail::formatChildren(ostr, settings, state, frame, items, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
