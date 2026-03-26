#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLWhereClause final : public DB::IAST {
 public:
  GQLWhereClause() = default;

  explicit GQLWhereClause(Ptr expression_) : expression(std::move(expression_)) {
    if (expression) children.push_back(expression);
  }

  String getID(char) const override { return "GQLWhereClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLWhereClause>(*this);
    result->children.clear();
    result->expression = expression ? expression->clone() : Ptr{};

    if (result->expression) result->children.push_back(result->expression);

    return result;
  }

  Ptr expression;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << "WHERE ";

    if (expression) expression->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &expression); }
};

}  // namespace DB::OPENGQL::AST
