#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLParenthesizedPathPattern final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLParenthesizedPathPattern"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLParenthesizedPathPattern>(*this);
    result->children.clear();
    result->subpath_variable = subpath_variable ? subpath_variable->clone() : Ptr{};
    result->prefix = prefix ? prefix->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};
    result->quantifier = quantifier ? quantifier->clone() : Ptr{};

    if (result->subpath_variable) result->children.push_back(result->subpath_variable);

    if (result->prefix) result->children.push_back(result->prefix);

    detail::cloneChildrenList(elements, result->elements, result->children);

    if (result->where) result->children.push_back(result->where);

    if (result->quantifier) result->children.push_back(result->quantifier);

    return result;
  }

  Ptr subpath_variable;
  Ptr prefix;
  PtrList elements;
  Ptr where;
  Ptr quantifier;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "(";

    if (subpath_variable) {
      subpath_variable->format(ostr, settings, state, frame);
      ostr << " = ";
    }

    if (prefix) {
      prefix->format(ostr, settings, state, frame);
      ostr << " ";
    }

    detail::formatChildren(ostr, settings, state, frame, elements, "");

    if (where) {
      ostr << " ";
      where->format(ostr, settings, state, frame);
    }

    ostr << ")";

    if (quantifier) quantifier->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &subpath_variable);
    f(nullptr, &prefix);

    for (auto &element : elements) f(nullptr, &element);

    f(nullptr, &where);
    f(nullptr, &quantifier);
  }
};

}  // namespace DB::OPENGQL::AST
