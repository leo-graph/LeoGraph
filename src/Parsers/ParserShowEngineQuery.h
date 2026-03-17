#pragma once

#include <Parsers/ASTShowEngineQuery.h>
#include <Parsers/CommonParsers.h>
#include <Parsers/ExpressionElementParsers.h>
#include <Parsers/IParserBase.h>

namespace DB {

/** Query SHOW ENGINES
 */
class ParserShowEnginesQuery : public IParserBase {
 protected:
  const char* getName() const override { return "SHOW ENGINES query"; }

  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override {
    auto query = make_intrusive<ASTShowEnginesQuery>();

    if (!ParserKeyword(Keyword::SHOW_ENGINES).ignore(pos, expected)) return false;

    node = query;

    return true;
  }
};

}  // namespace DB
