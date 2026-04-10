#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLForClause final : public DB::IAST {
 public:
  String alias;
  Ptr source;
  bool with_ordinality = false;
  bool with_offset = false;
  String ordinality_or_offset_alias;

  String getID(char) const override { return "GQLForClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLForClause>(*this);
    result->children.clear();
    result->source = source ? source->clone() : Ptr{};
    if (result->source) result->children.push_back(result->source);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override {
    ostr << "FOR " << alias << " IN ";

    if (source) source->format(ostr, settings, state, frame);

    if (!ordinality_or_offset_alias.empty()) {
      ostr << " WITH " << (with_ordinality ? "ORDINALITY " : "OFFSET ") << ordinality_or_offset_alias;
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &source); }
};

}  // namespace DB::OPENGQL::AST
