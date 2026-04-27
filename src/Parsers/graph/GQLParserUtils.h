#pragma once

#include <string_view>

namespace antlr4 {
class IntStream;
class ParserRuleContext;
}  // namespace antlr4

namespace DB::OPENGQL {

class GQLLexer;
class GQLParser;

class GQLParserUtils {
 public:
  static GQLLexer *getLexer(std::string_view query);
  static GQLParser *getParserLL(antlr4::IntStream *input);
  static GQLParser *getParserSLL(antlr4::IntStream *input);

  static void parseProgram(std::string_view query);
  static antlr4::ParserRuleContext *parseStatement(std::string_view query);
};

}  // namespace DB::OPENGQL
