#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLMatchStatementBlock final : public DB::IAST {
 public:
  bool parenthesized = false;
  PtrList matches;

  String getID(char) const override { return "GQLMatchStatementBlock"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLMatchStatementBlock>(*this);
    result->children.clear();
    detail::cloneChildrenList(matches, result->matches, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << (parenthesized ? "(" : "{");

    if (!matches.empty()) {
      ostr << " ";
      detail::formatChildren(ostr, settings, state, frame, matches, " ");
      ostr << " ";
    }

    ostr << (parenthesized ? ")" : "}");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &match : matches) f(nullptr, &match);
  }
};

}  // namespace DB::OPENGQL::AST
