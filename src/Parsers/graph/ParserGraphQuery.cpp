#include <Common/Exception.h>
#include <Parsers/CommonParsers.h>
#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/ParserGraphQuery.h>

namespace DB {

ASTPtr parseQuery(std::string_view query) {
  OPENGQL::GQLParserUtils::parseProgram(query);

  /// The visitor-based graph AST is being rebuilt separately.
  /// For now this entry point only sets up the ANTLR parsing pipeline.
  return {};
}

bool ParserGraphQuery::parseImpl(Pos &pos, ASTPtr &node, Expected &expected) {
  auto saved_pos = pos;

  if (!ParserKeyword(Keyword::MATCH).ignore(pos, expected)) {
    return false;
  }

  String gql_text = "MATCH ";
  auto current = pos;

  while (current.isValid()) {
    gql_text += String(current->begin, current->end);
    gql_text += " ";
    ++current;
  }

  try {
    node = parseQuery(gql_text);
  } catch (...) {
    pos = saved_pos;
    return false;
  }

  if (!node) {
    pos = saved_pos;
    return false;
  }

  while (pos.isValid()) {
    ++pos;
  }

  return true;
}

}  // namespace DB
