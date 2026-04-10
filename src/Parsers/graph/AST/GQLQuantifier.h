#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLQuantifier final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Star,
    Plus,
    Question,
    Exact,
    Range,
  };

  GQLQuantifier(Kind kind_, String lower_ = {}, String upper_ = {}) : kind(kind_), lower(std::move(lower_)), upper(std::move(upper_)) {}

  String getID(char) const override { return "GQLQuantifier"; }

  ASTPtr clone() const override { return make_intrusive<GQLQuantifier>(*this); }

  Kind kind;
  String lower;
  String upper;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override {
    switch (kind) {
      case Kind::Star:
        ostr << "*";
        return;
      case Kind::Plus:
        ostr << "+";
        return;
      case Kind::Question:
        ostr << "?";
        return;
      case Kind::Exact:
        ostr << "{" << lower << "}";
        return;
      case Kind::Range:
        ostr << "{" << lower << ", " << upper << "}";
        return;
    }
  }
};

}  // namespace DB::OPENGQL::AST
