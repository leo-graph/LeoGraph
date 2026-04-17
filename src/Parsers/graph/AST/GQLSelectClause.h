#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLSelectClause final : public DB::IAST {
 public:
  bool distinct = false;
  bool select_all = false;
  PtrList items;
  Ptr source;
  Ptr where;
  Ptr group_by;
  Ptr having;

  String getID(char) const override { return "GQLSelectClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLSelectClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    result->source = source ? source->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};
    result->group_by = group_by ? group_by->clone() : Ptr{};
    result->having = having ? having->clone() : Ptr{};

    if (result->source) result->children.push_back(result->source);

    if (result->where) result->children.push_back(result->where);

    if (result->group_by) result->children.push_back(result->group_by);

    if (result->having) result->children.push_back(result->having);

    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "SELECT";

    if (distinct) ostr << " DISTINCT";

    ostr << " ";

    if (select_all)
      ostr << "*";
    else
      detail::formatChildren(ostr, settings, state, frame, items, ", ");

    if (source) {
      ostr << " FROM ";
      source->format(ostr, settings, state, frame);
    }

    if (where) {
      ostr << " ";
      where->format(ostr, settings, state, frame);
    }

    if (group_by) {
      ostr << " ";
      group_by->format(ostr, settings, state, frame);
    }

    if (having) {
      ostr << " ";
      having->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);

    f(nullptr, &source);
    f(nullptr, &where);
    f(nullptr, &group_by);
    f(nullptr, &having);
  }
};

}  // namespace DB::OPENGQL::AST
