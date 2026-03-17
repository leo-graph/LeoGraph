#pragma once

#include <Parsers/IParserBase.h>
#include <Parsers/Kusto/ParserKQLQuery.h>
#include <unordered_map>
namespace DB {

class KQLOperators {
 public:
  static bool convert(std::vector<String>& tokens, IParser::Pos& pos);

 protected:
  enum class WildcardsPos : uint8_t { none, left, right, both };

  static String genHaystackOpExpr(std::vector<String>& tokens, IParser::Pos& token_pos, String kql_op, String ch_op,
                                  WildcardsPos wildcards_pos, WildcardsPos space_pos = WildcardsPos::none);
  static String genHasAnyAllOpExpr(std::vector<String>& tokens, IParser::Pos& token_pos, String kql_op, String ch_op);
};

}  // namespace DB
