#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLCallClauseBase : public DB::IAST {
 public:
  GQLCallClauseBase() = default;
  GQLCallClauseBase(const GQLCallClauseBase &) = default;
  GQLCallClauseBase &operator=(const GQLCallClauseBase &) = default;
  virtual ~GQLCallClauseBase() override = default;

  String getID(char) const override = 0;
  ASTPtr clone() const override = 0;

  bool optional = false;

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override = 0;

  void formatCallPrefix(WriteBuffer& ostr) const {
    if (optional) ostr << "OPTIONAL ";

    ostr << "CALL ";
  }
};

}  // namespace DB::OPENGQL::AST
