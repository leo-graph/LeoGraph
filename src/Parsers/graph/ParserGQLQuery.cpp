#include <Parsers/ASTSetQuery.h>
#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>
#include <Parsers/graph/ParserGQLQuery.h>
#include <Parsers/ParserSetQuery.h>

#include <Common/Exception.h>

namespace DB {

namespace {

bool isDialectSwitchSet(const ASTPtr& ast) {
  const auto* set = ast->as<ASTSetQuery>();
  if (!set) return false;
  if (!set->default_settings.empty() || !set->query_parameters.empty()) return false;
  for (const auto& change : set->changes) {
    if (change.name != "dialect" && change.name != "query_language") return false;
  }
  return !set->changes.empty();
}

}  // namespace

bool ParserGQLQuery::parseImpl(Pos& pos, ASTPtr& node, Expected& expected) {
  auto before_set = pos;
  ParserSetQuery set_p;
  ASTPtr set_node;
  if (set_p.parse(pos, set_node, expected) && isDialectSwitchSet(set_node)) {
    node = std::move(set_node);
    return true;
  }
  pos = before_set;

  const auto* begin = pos->begin;

  while (!pos->isEnd() && pos->type != TokenType::Semicolon) ++pos;

  const auto* end = pos->begin;
  std::string_view query_text(begin, static_cast<size_t>(end - begin));

  OPENGQL::GQLParseTreeVisitor visitor;
  auto* parse_tree = OPENGQL::GQLParserUtils::parseStatement(query_text);
  node = std::any_cast<ASTPtr>(visitor.visit(parse_tree));

  return node != nullptr;
}

}  // namespace DB
