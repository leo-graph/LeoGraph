#include <Common/quoteString.h>
#include <IO/Operators.h>
#include <Parsers/Access/ASTPublicSSHKey.h>

namespace DB {

void ASTPublicSSHKey::formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const {
  ostr << "KEY ";
  ostr << backQuoteIfNeed(key_base64) << ' ';
  ostr << "TYPE ";
  ostr << backQuoteIfNeed(type);
}

}  // namespace DB
