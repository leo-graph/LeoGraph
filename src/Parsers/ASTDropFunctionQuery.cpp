#include <Common/quoteString.h>
#include <IO/Operators.h>
#include <Parsers/ASTDropFunctionQuery.h>

namespace DB {

ASTPtr ASTDropFunctionQuery::clone() const { return make_intrusive<ASTDropFunctionQuery>(*this); }

void ASTDropFunctionQuery::formatImpl(WriteBuffer &ostr, const IAST::FormatSettings &settings, IAST::FormatState &,
                                      IAST::FormatStateStacked) const {
  ostr << "DROP FUNCTION ";

  if (if_exists) ostr << "IF EXISTS ";

  ostr << backQuoteIfNeed(function_name);
  formatOnCluster(ostr, settings);
}

}  // namespace DB
