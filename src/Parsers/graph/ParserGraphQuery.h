#pragma once

#include <Parsers/IParserBase.h>

namespace DB {

class ParserGraphQuery : public IParserBase {
 protected:
  const char* getName() const override { return "Graph query (GQL)"; }
  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override;
};

ASTPtr parseQuery(std::string_view query, bool use_statement_rule = false);

}  // namespace DB
