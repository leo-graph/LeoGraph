#pragma once

#include <Parsers/IAST.h>

namespace DB::OPENGQL::AST {

class GQLFinishClause final : public DB::IAST {
 public:
  String getID(char) const override { return "GQLFinishClause"; }
  ASTPtr clone() const override { return make_intrusive<GQLFinishClause>(); }

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override { ostr << "FINISH"; }
};

}  // namespace DB::OPENGQL::AST
