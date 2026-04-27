#include <Parsers/CommonParsers.h>
#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>
#include <Parsers/graph/ParserGraphQuery.h>

#include <Common/Exception.h>

#include <cctype>

namespace DB {

namespace {

String collectRemainingQueryText(IParserBase::Pos pos) {
  String gql_text;

  while (pos.isValid()) {
    gql_text += String(pos->begin, pos->end);
    gql_text += " ";
    ++pos;
  }

  return gql_text;
}

String toUpperASCII(String text) {
  for (auto& ch : text) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

  return text;
}

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

bool looksLikeGraphSelect(const String& upper_text) {
  auto from_pos = upper_text.find(" FROM ");
  if (from_pos == String::npos) return false;

  auto tail = std::string_view(upper_text).substr(from_pos + 6);
  return tail.find(" MATCH ") != std::string_view::npos || tail.find(" OPTIONAL MATCH ") != std::string_view::npos ||
         tail.find('{') != std::string_view::npos;
}

bool looksLikeFocusedUseQuery(const String& upper_text) {
  if (!startsWith(upper_text, "USE ")) return false;

  static constexpr std::array<std::string_view, 11> suffix_markers = {
      " MATCH ", " OPTIONAL MATCH ", " RETURN ", " FINISH ", " SELECT ", " CALL ", " OPTIONAL CALL ", " LET ", " FOR ", " FILTER ", " {",
  };

  for (const auto marker : suffix_markers) {
    if (upper_text.find(marker, 4) != String::npos) return true;
  }

  return false;
}

bool looksLikeGraphQuery(const String& upper_text) {
  if (startsWith(upper_text, "OPTIONAL MATCH ") || startsWith(upper_text, "MATCH ")) return true;

  if (startsWith(upper_text, "OPTIONAL CALL ") || startsWith(upper_text, "CALL ")) return true;

  if (looksLikeFocusedUseQuery(upper_text)) return true;

  if (startsWith(upper_text, "SELECT ") && looksLikeGraphSelect(upper_text)) return true;

  return false;
}

bool isWeakGraphPrefix(const String& upper_text) { return startsWith(upper_text, "USE ") || startsWith(upper_text, "SELECT "); }

}  // namespace

ASTPtr parseQuery(std::string_view query, bool use_statement_rule) {
  OPENGQL::GQLParseTreeVisitor visitor;

  if (use_statement_rule) {
    auto* parse_tree = OPENGQL::GQLParserUtils::parseStatement(query);
    return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
  }

  auto* parse_tree = OPENGQL::GQLParserUtils::parseCompositeQueryStatement(query);
  return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}

bool ParserGraphQuery::parseImpl(Pos& pos, ASTPtr& node, Expected& expected) {
  (void)expected;

  auto current = pos;
  String gql_text = collectRemainingQueryText(current);
  String upper_text = toUpperASCII(gql_text);

  if (!looksLikeGraphQuery(upper_text)) return false;

  try {
    node = parseQuery(gql_text, true);
  } catch (const DB::Exception&) {
    if (isWeakGraphPrefix(upper_text)) return false;
    throw;
  }

  if (!node) return false;

  while (current.isValid()) ++current;
  pos = current;
  return true;
}

}  // namespace DB
