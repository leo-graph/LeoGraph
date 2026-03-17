#pragma once

#include <Parsers/ExpressionElementParsers.h>
#include <Parsers/IParserBase.h>

namespace DB {

/** Query like this:
 * UNDROP TABLE [db.]name [UUID uuid]
 */
class ParserUndropQuery : public IParserBase {
 protected:
  const char* getName() const override { return "UNDROP query"; }
  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override;
};

}  // namespace DB
