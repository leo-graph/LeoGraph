#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma clang diagnostic ignored "-Wdocumentation-html"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#include <BaseErrorListener.h>
#pragma clang diagnostic pop

namespace DB::OPENGQL {

class LexerErrorListener : public antlr4::BaseErrorListener {
 public:
  void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offending_symbol, size_t line, size_t pos, const std::string& message,
                   std::exception_ptr e) override;
};

}  // namespace DB::OPENGQL
