#include <Common/quoteString.h>
#include <Parsers/ASTShowFunctionsQuery.h>

namespace DB {

ASTPtr ASTShowFunctionsQuery::clone() const {
  auto res = make_intrusive<ASTShowFunctionsQuery>(*this);
  res->children.clear();
  cloneOutputOptions(*res);
  return res;
}

void ASTShowFunctionsQuery::formatQueryImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const {
  ostr << "SHOW FUNCTIONS";
  if (!like.empty()) ostr << (case_insensitive_like ? " ILIKE " : " LIKE ") << quoteString(like);
}

}  // namespace DB
