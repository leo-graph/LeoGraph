#include <Common/Exception.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma clang diagnostic ignored "-Wdocumentation-html"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#include <GQLParser.h>
#pragma clang diagnostic pop

#include <Parsers/graph/ParserErrorListener.h>
#include <Token.h>

namespace DB::ErrorCodes {
extern int SYNTAX_ERROR;
}

namespace DB::OPENGQL {

using namespace antlr4;

void ParserErrorListener::syntaxError(Recognizer *recognizer, Token *token, size_t, size_t, const std::string &message,
                                      std::exception_ptr) {
  auto *parser = dynamic_cast<GQLParser *>(recognizer);

  if (!parser || !token) {
    throw DB::Exception(ErrorCodes::SYNTAX_ERROR, "Can't parse input, {}", message);
  }

  throw DB::Exception(ErrorCodes::SYNTAX_ERROR, "Can't parse input, Last element parsed so far:\n{}\nParser error: (pos {}) {}",
                      parser->getRuleContext()->toStringTree(parser, true), token->getStartIndex(), message);
}

}  // namespace DB::OPENGQL
