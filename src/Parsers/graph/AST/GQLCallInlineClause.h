#pragma once

#include <Parsers/graph/AST/GQLCallClauseBase.h>

namespace DB::OPENGQL::AST {

class GQLCallInlineClause final : public GQLCallClauseBase {
 public:
  Ptr variable_scope;
  Ptr subquery;

  String getID(char) const override { return "GQLCallInlineClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLCallInlineClause>(*this);
    result->children.clear();
    result->variable_scope = variable_scope ? variable_scope->clone() : Ptr{};
    result->subquery = subquery ? subquery->clone() : Ptr{};

    if (result->variable_scope) result->children.push_back(result->variable_scope);

    if (result->subquery) result->children.push_back(result->subquery);

    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    formatCallPrefix(ostr);

    if (variable_scope) {
      variable_scope->format(ostr, settings, state, frame);

      if (subquery) ostr << " ";
    }

    if (subquery) subquery->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &variable_scope);
    f(nullptr, &subquery);
  }
};

}  // namespace DB::OPENGQL::AST
