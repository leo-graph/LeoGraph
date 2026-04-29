#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLCallVariableScopeClause final : public DB::IAST {
 public:
  PtrList variables;

  String getID(char) const override { return "GQLCallVariableScopeClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLCallVariableScopeClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(variables, result->variables, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "(";
    detail::formatChildren(ostr, settings, state, frame, variables, ", ");
    ostr << ")";
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &variable : variables) f(nullptr, &variable);
  }
};

}  // namespace DB::OPENGQL::AST
