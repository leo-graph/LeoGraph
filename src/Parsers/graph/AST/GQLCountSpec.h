#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

enum class CountSpecKind : UInt8 {
  Integer,
  DynamicParameter,
};

class GQLCountSpec final : public DB::IAST {
 public:
  explicit GQLCountSpec(CountSpecKind kind_, String text_) : kind(kind_), text(std::move(text_)) {}

  String getID(char) const override { return "GQLCountSpec"; }

  ASTPtr clone() const override { return make_intrusive<GQLCountSpec>(*this); }

  CountSpecKind kind;
  String text;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override { ostr << text; }
};

}  // namespace DB::OPENGQL::AST
