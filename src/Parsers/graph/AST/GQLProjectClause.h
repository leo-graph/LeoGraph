#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLProjectClause final : public DB::IAST {
 public:
  enum class Type : UInt8 {
    Return,
  };

  GQLProjectClause() : type(Type::Return) {}

  explicit GQLProjectClause(Type type_) : type(type_) {}

  String getID(char) const override { return "GQLProjectClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLProjectClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    result->order_by = order_by ? order_by->clone() : Ptr{};
    result->offset = offset ? offset->clone() : Ptr{};
    result->limit = limit ? limit->clone() : Ptr{};

    if (result->order_by) result->children.push_back(result->order_by);
    if (result->offset) result->children.push_back(result->offset);
    if (result->limit) result->children.push_back(result->limit);

    return result;
  }

  Type type;
  bool distinct = false;
  bool return_all = false;
  PtrList items;
  Ptr order_by;
  Ptr offset;
  Ptr limit;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "RETURN";

    if (distinct) ostr << " DISTINCT";

    ostr << " ";

    if (return_all)
      ostr << "*";
    else
      detail::formatChildren(ostr, settings, state, frame, items, ", ");

    if (order_by) {
      ostr << " ";
      order_by->format(ostr, settings, state, frame);
    }

    if (offset) {
      ostr << " OFFSET ";
      offset->format(ostr, settings, state, frame);
    }

    if (limit) {
      ostr << " LIMIT ";
      limit->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &item : items) f(nullptr, &item);

    f(nullptr, &order_by);
    f(nullptr, &offset);
    f(nullptr, &limit);
  }
};

}  // namespace DB::OPENGQL::AST
