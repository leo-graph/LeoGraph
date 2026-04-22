#pragma once

#include <Parsers/IParserBase.h>

namespace DB {

/// Dialect-mode GQL parser: used when `SET dialect = 'gql'`
/// (or equivalently `SET query_language = 'gql'`).
/// Unlike ParserGraphQuery it does NOT rely on prefix heuristics --
/// the entire query text is unconditionally routed through the ANTLR
/// GQL `statement` grammar rule.
class ParserGQLQuery final : public IParserBase {
 protected:
  const char* getName() const override { return "GQL dialect query"; }
  bool parseImpl(Pos& pos, ASTPtr& node, Expected& expected) override;
};

}  // namespace DB
