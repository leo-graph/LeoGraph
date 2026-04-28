#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/GQLParseTreeVisitor.h>
#include <Parsers/graph/ParserGQLQuery.h>

#include <Common/Exception.h>
#include <Parsers/IAST.h>

#include <any>
#include <cctype>
#include <string_view>

namespace DB {

namespace ErrorCodes {
extern const int SYNTAX_ERROR;
}

namespace {

bool isASCIISpace(char c)
{
  return std::isspace(static_cast<unsigned char>(c));
}

std::string_view makeSingleStatementView(const char* begin, const char* end)
{
  std::string_view query(begin, static_cast<size_t>(end - begin));

  while (!query.empty() && isASCIISpace(query.front()))
    query.remove_prefix(1);

  while (!query.empty() && isASCIISpace(query.back()))
    query.remove_suffix(1);

  return query;
}

}  // namespace

ASTPtr ParserGQLQuery::parseStatementText(std::string_view query_text) const
{
  OPENGQL::GQLParseTreeVisitor visitor;
  auto* parse_tree = OPENGQL::GQLParserUtils::parseStatement(query_text);
  return std::any_cast<ASTPtr>(visitor.visit(parse_tree));
}

ASTPtr tryParseGQLQuery(ParserGQLQuery& parser,
                        const char*& out_query_end,
                        const char* end,
                        std::string& out_error_message,
                        int* out_error_code,
                        bool hilite,
                        const std::string& description,
                        bool allow_multi_statements,
                        size_t max_query_size,
                        size_t max_parser_depth,
                        size_t max_parser_backtracks)
{
  (void)hilite;
  (void)description;
  (void)allow_multi_statements;
  (void)max_parser_backtracks;

  if (out_error_code)
    *out_error_code = ErrorCodes::SYNTAX_ERROR;

  const char* query_begin = out_query_end;

  try
  {
    std::string_view query_text = makeSingleStatementView(query_begin, end);
    if (query_text.empty())
    {
      out_error_message = "Empty query";
      out_query_end = end;
      return nullptr;
    }

    if (max_query_size && query_text.size() > max_query_size)
      throw Exception(ErrorCodes::SYNTAX_ERROR, "Max query size exceeded (can be increased with the `max_query_size` setting)");

    ASTPtr res = parser.parseStatementText(query_text);
    if (!res)
      throw Exception(ErrorCodes::SYNTAX_ERROR, "Cannot parse GQL query");

    if (res && max_parser_depth)
      res->checkDepth(max_parser_depth);

    out_query_end = end;
    return res;
  }
  catch (const Exception& e)
  {
    out_error_message = e.message();
    if (out_error_code)
      *out_error_code = e.code();
    out_query_end = query_begin;
    return nullptr;
  }
  catch (const std::exception& e)
  {
    out_error_message = e.what();
    out_query_end = query_begin;
    return nullptr;
  }
}

ASTPtr parseGQLQueryAndMovePosition(ParserGQLQuery& parser,
                                    const char*& pos,
                                    const char* end,
                                    const std::string& query_description,
                                    bool allow_multi_statements,
                                    size_t max_query_size,
                                    size_t max_parser_depth,
                                    size_t max_parser_backtracks)
{
  std::string error_message;
  int error_code = ErrorCodes::SYNTAX_ERROR;
  ASTPtr res = tryParseGQLQuery(parser, pos, end, error_message, &error_code, false, query_description, allow_multi_statements,
                                max_query_size, max_parser_depth, max_parser_backtracks);

  if (res)
    return res;

  throw Exception::createDeprecated(error_message, error_code);
}

ASTPtr parseGQLQuery(ParserGQLQuery& parser,
                     const char* begin,
                     const char* end,
                     const std::string& query_description,
                     size_t max_query_size,
                     size_t max_parser_depth,
                     size_t max_parser_backtracks)
{
  return parseGQLQueryAndMovePosition(parser, begin, end, query_description, false, max_query_size, max_parser_depth, max_parser_backtracks);
}

ASTPtr parseGQLQuery(ParserGQLQuery& parser,
                     const std::string& query,
                     const std::string& query_description,
                     size_t max_query_size,
                     size_t max_parser_depth,
                     size_t max_parser_backtracks)
{
  return parseGQLQuery(parser, query.data(), query.data() + query.size(), query_description, max_query_size, max_parser_depth,
                       max_parser_backtracks);
}

ASTPtr parseGQLQuery(ParserGQLQuery& parser, const std::string& query, size_t max_query_size, size_t max_parser_depth,
                     size_t max_parser_backtracks)
{
  return parseGQLQuery(parser, query, "GQL dialect query", max_query_size, max_parser_depth, max_parser_backtracks);
}

}  // namespace DB
