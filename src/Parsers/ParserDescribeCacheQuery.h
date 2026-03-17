#pragma once

#include <Parsers/ExpressionElementParsers.h>
#include <Parsers/IParserBase.h>

namespace DB {

/** Query (DESCRIBE | DESC) FILESYSTEM CACHE 'cache_name'
 */
class ParserDescribeCacheQuery : public IParserBase {
 protected:
  const char* getName() const override { return "DESCRIBE FILESYSTEM CACHE query"; }
  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override;
};

}  // namespace DB
