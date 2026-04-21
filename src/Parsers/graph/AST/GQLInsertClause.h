#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLInsertPathPattern final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLInsertPathPattern"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLInsertPathPattern>(*this);
    result->children.clear();
    detail::cloneChildrenList(elements, result->elements, result->children);
    return result;
  }

  PtrList elements;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    detail::formatChildren(ostr, settings, state, frame, elements, "");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &element : elements) f(nullptr, &element);
  }
};

class GQLInsertClause final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLInsertClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLInsertClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(path_patterns, result->path_patterns, result->children);
    return result;
  }

  PtrList path_patterns;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "INSERT ";
    detail::formatChildren(ostr, settings, state, frame, path_patterns, ", ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &pattern : path_patterns) f(nullptr, &pattern);
  }
};

}  // namespace DB::OPENGQL::AST
