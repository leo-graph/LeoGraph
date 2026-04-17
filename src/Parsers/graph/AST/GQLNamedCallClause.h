#pragma once

#include <Parsers/graph/AST/GQLCallClause.h>

namespace DB::OPENGQL::AST {

class GQLNamedCallClause final : public GQLCallClause {
 public:
  Ptr procedure;
  PtrList arguments;
  Ptr yield;

  String getID(char) const override { return "GQLNamedCallClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLNamedCallClause>(*this);
    result->children.clear();
    result->procedure = procedure ? procedure->clone() : Ptr{};
    detail::cloneChildrenList(arguments, result->arguments, result->children);
    result->yield = yield ? yield->clone() : Ptr{};

    if (result->procedure) result->children.insert(result->children.begin(), result->procedure);

    if (result->yield) result->children.push_back(result->yield);

    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    formatCallPrefix(ostr);

    if (procedure) procedure->format(ostr, settings, state, frame);

    ostr << "(";
    detail::formatChildren(ostr, settings, state, frame, arguments, ", ");
    ostr << ")";

    if (yield) {
      ostr << " ";
      yield->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &procedure);

    for (auto &argument : arguments) f(nullptr, &argument);

    f(nullptr, &yield);
  }
};

}  // namespace DB::OPENGQL::AST
