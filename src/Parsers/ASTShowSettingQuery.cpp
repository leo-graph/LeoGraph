#include <Parsers/ASTShowSettingQuery.h>

#include <Common/quoteString.h>
#include <IO/Operators.h>
#include <iomanip>

namespace DB {

ASTPtr ASTShowSettingQuery::clone() const {
  auto res = make_intrusive<ASTShowSettingQuery>(*this);
  res->children.clear();
  cloneOutputOptions(*res);
  res->setting_name = setting_name;
  return res;
}

void ASTShowSettingQuery::formatQueryImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const {
  ostr << "SHOW SETTING " << backQuoteIfNeed(setting_name);
}

}  // namespace DB
