#include <Parsers/CommonParsers.h>
#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>
#include <Parsers/graph/ParserGraphQuery.h>

namespace DB {

namespace {

bool tryParseGraphQueryPrefix(IParserBase::Pos& pos, Expected& expected, String& gql_text) {
  auto current = pos;

  if (ParserKeyword::createDeprecated("OPTIONAL MATCH").ignore(current, expected)) {
    pos = current;
    gql_text = "OPTIONAL MATCH ";
    return true;
  }

  if (ParserKeyword(Keyword::MATCH).ignore(current, expected)) {
    pos = current;
    gql_text = "MATCH ";
    return true;
  }

  return false;
}

}  // namespace

ASTPtr parseQuery(std::string_view query) {
  auto* parse_tree = OPENGQL::GQLParserUtils::parseCompositeQueryStatement(query);
  OPENGQL::GQLParseTreeVisitor visitor;
  return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}

bool ParserGraphQuery::parseImpl(Pos& pos, ASTPtr& node, Expected& expected) {
  auto current = pos;
  String gql_text;

  if (!tryParseGraphQueryPrefix(current, expected, gql_text)) return false;

  auto end = current;

  while (end.isValid()) {
    gql_text += String(end->begin, end->end);
    gql_text += " ";
    ++end;
  }

  node = parseQuery(gql_text);

  if (!node) return false;

  pos = end;
  return true;
}

}  // namespace DB
