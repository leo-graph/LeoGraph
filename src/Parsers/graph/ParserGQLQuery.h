#pragma once

#include <Parsers/IAST_fwd.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace DB {

/// Dialect-mode GQL parser: used when `SET dialect = 'gql'`
/// (or equivalently `SET query_language = 'gql'`).
/// This is the production GQL entry point: the entire query text is
/// unconditionally routed through the ANTLR GQL `statement` grammar rule.
class ParserGQLQuery final
{
 public:
  ASTPtr parseStatementText(std::string_view query_text) const;
};

/// GQL query parser entry points
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
                        size_t max_parser_backtracks);

ASTPtr parseGQLQueryAndMovePosition(ParserGQLQuery& parser,
                                    const char*& pos,
                                    const char* end,
                                    const std::string& description,
                                    bool allow_multi_statements,
                                    size_t max_query_size,
                                    size_t max_parser_depth,
                                    size_t max_parser_backtracks);

ASTPtr parseGQLQuery(ParserGQLQuery& parser,
                     const char* begin,
                     const char* end,
                     const std::string& description,
                     size_t max_query_size,
                     size_t max_parser_depth,
                     size_t max_parser_backtracks);

ASTPtr parseGQLQuery(ParserGQLQuery& parser,
                     const std::string& query,
                     const std::string& query_description,
                     size_t max_query_size,
                     size_t max_parser_depth,
                     size_t max_parser_backtracks);

ASTPtr parseGQLQuery(ParserGQLQuery& parser,
                     const std::string& query,
                     size_t max_query_size,
                     size_t max_parser_depth,
                     size_t max_parser_backtracks);

}  // namespace DB
