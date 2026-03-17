#pragma once

#include <Parsers/ExpressionElementParsers.h>
#include <Parsers/IParserBase.h>

namespace DB {

/** Query (EXISTS | SHOW CREATE) [DATABASE|TABLE|DICTIONARY] [db.]name [FORMAT format]
 */
class ParserTablePropertiesQuery : public IParserBase {
 protected:
  const char* getName() const override { return "EXISTS or SHOW CREATE query"; }
  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override;
};

}  // namespace DB
