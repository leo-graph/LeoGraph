#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLReturnClause final : public DB::IAST {
 public:
  bool distinct = false;
  bool return_all = false;
  PtrList items;
  Ptr group_by;

  String getID(char) const override { return "GQLReturnClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLReturnClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    result->group_by = group_by ? group_by->clone() : Ptr{};

    if (result->group_by) result->children.push_back(result->group_by);

    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "RETURN";

    if (distinct) ostr << " DISTINCT";

    ostr << " ";

    if (return_all)
      ostr << "*";
    else
      detail::formatChildren(ostr, settings, state, frame, items, ", ");

    if (group_by) {
      ostr << " ";
      group_by->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);

    f(nullptr, &group_by);
  }
};

}  // namespace DB::OPENGQL::AST
