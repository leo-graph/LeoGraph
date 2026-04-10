#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLCallClause final : public DB::IAST {
 public:
  bool optional = false;
  bool inline_procedure = false;
  Ptr procedure;
  PtrList arguments;
  PtrList yield_items;

  String getID(char) const override { return "GQLCallClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLCallClause>(*this);
    result->children.clear();
    result->procedure = procedure ? procedure->clone() : Ptr{};
    if (result->procedure) result->children.push_back(result->procedure);
    detail::cloneChildrenList(arguments, result->arguments, result->children);
    detail::cloneChildrenList(yield_items, result->yield_items, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override {
    if (optional) ostr << "OPTIONAL ";

    ostr << "CALL ";

    if (procedure) procedure->format(ostr, settings, state, frame);

    if (!inline_procedure)
    {
      ostr << "(";
      detail::formatChildren(ostr, settings, state, frame, arguments, ", ");
      ostr << ")";
    }

    if (!yield_items.empty()) {
      ostr << " YIELD ";
      detail::formatChildren(ostr, settings, state, frame, yield_items, ", ");
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &procedure);
    for (auto & argument : arguments) f(nullptr, &argument);
    for (auto & item : yield_items) f(nullptr, &item);
  }
};

}  // namespace DB::OPENGQL::AST
