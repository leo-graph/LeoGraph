#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLCallClause : public DB::IAST {
 public:
  bool optional = false;

 protected:
  void formatCallPrefix(WriteBuffer& ostr) const {
    if (optional) ostr << "OPTIONAL ";

    ostr << "CALL ";
  }
};

}  // namespace DB::OPENGQL::AST
