#include <Common/Exception.h>
#include <fmt/format.h>
#include <Parsers/graph/LexerErrorListener.h>
#include <string>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma clang diagnostic ignored "-Wdocumentation-html"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#include <Token.h>
#pragma clang diagnostic pop

namespace DB::ErrorCodes {
extern int SYNTAX_ERROR;
}

namespace DB::OPENGQL {

using namespace antlr4;

namespace {

std::string getHexString(std::string_view value) {
  static constexpr char hex_digits[] = "0123456789abcdef";
  std::string hex;
  hex.resize(value.size() * 2);

  for (size_t i = 0; i < value.size(); ++i) {
    const auto byte = static_cast<unsigned char>(value[i]);
    hex[i * 2] = hex_digits[byte >> 4];
    hex[i * 2 + 1] = hex_digits[byte & 0x0F];
  }

  return hex;
}

}  // namespace

void LexerErrorListener::syntaxError(Recognizer *, Token *offending_symbol, size_t, size_t, const std::string &message,
                                     std::exception_ptr) {
  std::string msg;

  if (offending_symbol) {
    std::string token = offending_symbol->getText();
    std::string hex = getHexString(token);
    msg = fmt::format("offending symbol: {}, hex: {}.", token, hex);

    throw DB::Exception(ErrorCodes::SYNTAX_ERROR, "Can't recognize input: {}, {}", message, msg);
  }

  throw DB::Exception(ErrorCodes::SYNTAX_ERROR, "Can't recognize input, {}", message);
}

}  // namespace DB::OPENGQL
