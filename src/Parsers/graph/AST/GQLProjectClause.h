#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLProjectClause final : public DB::IAST {
 public:
  enum class Type : UInt8 {
    Return,
    Select,
  };

  GQLProjectClause() : type(Type::Return) {}

  explicit GQLProjectClause(Type type_) : type(type_) {}

  String getID(char) const override { return "GQLProjectClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLProjectClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(items, result->items, result->children);
    result->source = source ? source->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};
    result->group_by = group_by ? group_by->clone() : Ptr{};
    result->having = having ? having->clone() : Ptr{};
    result->order_by = order_by ? order_by->clone() : Ptr{};
    result->offset = offset ? offset->clone() : Ptr{};
    result->limit = limit ? limit->clone() : Ptr{};

    if (result->source) result->children.push_back(result->source);
    if (result->where) result->children.push_back(result->where);
    if (result->group_by) result->children.push_back(result->group_by);
    if (result->having) result->children.push_back(result->having);
    if (result->order_by) result->children.push_back(result->order_by);
    if (result->offset) result->children.push_back(result->offset);
    if (result->limit) result->children.push_back(result->limit);

    return result;
  }

  Type type;
  bool distinct = false;
  bool return_all = false;
  PtrList items;
  Ptr source;
  Ptr where;
  Ptr group_by;
  Ptr having;
  Ptr order_by;
  Ptr offset;
  Ptr limit;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << (type == Type::Select ? "SELECT" : "RETURN");

    if (distinct) ostr << " DISTINCT";

    ostr << " ";

    if (return_all)
      ostr << "*";
    else
      detail::formatChildren(ostr, settings, state, frame, items, ", ");

    if (source) {
      ostr << " ";
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

    f(nullptr, &source);
    f(nullptr, &where);
    f(nullptr, &group_by);
    f(nullptr, &having);
    f(nullptr, &order_by);
    f(nullptr, &offset);
    f(nullptr, &limit);
  }
};

}  // namespace DB::OPENGQL::AST
