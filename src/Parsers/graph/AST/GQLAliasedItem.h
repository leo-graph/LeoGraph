#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLAliasedItem final : public DB::IAST {
 public:
  GQLAliasedItem(Ptr expression_, String alias_ = {}) : expression(std::move(expression_)), alias(std::move(alias_)) {
    if (expression) children.push_back(expression);
  }

  String getID(char) const override { return "GQLAliasedItem"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLAliasedItem>(*this);
    result->children.clear();
    result->expression = expression ? expression->clone() : Ptr{};

    if (result->expression) result->children.push_back(result->expression);

    return result;
  }

  Ptr expression;
  String alias;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (expression) expression->format(ostr, settings, state, frame);

    if (!alias.empty()) {
      ostr << " AS ";
      settings.writeIdentifier(ostr, alias, false);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &expression); }
};

}  // namespace DB::OPENGQL::AST
