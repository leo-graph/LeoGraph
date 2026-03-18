#include <Common/Exception.h>
#include <Parsers/graph/GQLParserUtils.h>
#include <Parsers/graph/LexerErrorListener.h>
#include <Parsers/graph/ParserErrorListener.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma clang diagnostic ignored "-Wdocumentation-html"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Winconsistent-missing-destructor-override"
#pragma clang diagnostic ignored "-Wshadow-field"
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wsuggest-destructor-override"
#include <ANTLRInputStream.h>
#include <BailErrorStrategy.h>
#include <CommonTokenStream.h>
#include <DefaultErrorStrategy.h>
#include <GQLLexer.h>
#include <GQLParser.h>
#pragma clang diagnostic pop

namespace DB {

namespace ErrorCodes {
extern const int CANNOT_PARSE_QUERY;
extern const int LOGICAL_ERROR;
extern const int INPUT_TEXT_INVALID_UTF8;
}  // namespace ErrorCodes

using namespace antlr4;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
extern "C" {
#ifdef ADDRESS_SANITIZER
void __lsan_ignore_object(const void *);
#else
void __lsan_ignore_object(const void *) {}  // NOLINT
#endif
}
#pragma clang diagnostic pop

namespace {

ANTLRInputStream *getLexerInput() {
  static thread_local ANTLRInputStream *input;

  if (unlikely(!input)) {
    input = new ANTLRInputStream();
    __lsan_ignore_object(input);
  }

  return input;
}

OPENGQL::GQLLexer *getLexerImpl() {
  static thread_local OPENGQL::GQLLexer *lexer;
  static thread_local OPENGQL::LexerErrorListener *lexer_error_listener;
  ANTLRInputStream *input = getLexerInput();

  if (unlikely(!lexer)) {
    lexer = new OPENGQL::GQLLexer(input);
    __lsan_ignore_object(lexer);

    lexer_error_listener = new OPENGQL::LexerErrorListener();
    __lsan_ignore_object(lexer_error_listener);

    lexer->removeErrorListeners();
    lexer->addErrorListener(lexer_error_listener);
  }

  return lexer;
}

template <typename Context>
Context *parseWithFallback(std::string_view query, Context *(OPENGQL::GQLParser::*parse_rule)()) {
  CommonTokenStream tokens(OPENGQL::GQLParserUtils::getLexer(query));
  OPENGQL::GQLParser *parser = OPENGQL::GQLParserUtils::getParserSLL(&tokens);

  try {
    return (parser->*parse_rule)();
  } catch (ParseCancellationException &) {
    parser->reset();
    parser = OPENGQL::GQLParserUtils::getParserLL(&tokens);
    return (parser->*parse_rule)();
  } catch (Exception &e) {
    e.addMessage("\nerror when parse gql query: {}.", query);
    throw;
  } catch (...) {
    throw Exception(ErrorCodes::CANNOT_PARSE_QUERY, "error when parse query: {}.", query);
  }
}

}  // namespace

namespace OPENGQL {

GQLLexer *GQLParserUtils::getLexer(std::string_view query) {
  auto *input = getLexerInput();

  try {
    input->load(query.data(), query.size());
  } catch (std::exception &e) {
    throw Exception(ErrorCodes::LOGICAL_ERROR, "parse utf8 input text failed. {}. input: {}", e.what(), query.substr(0, 4096));
  }

  auto *lexer = getLexerImpl();
  lexer->reset();
  return lexer;
}

GQLParser *GQLParserUtils::getParserLL(IntStream *input) {
  static thread_local GQLParser *parser;
  static thread_local ParserErrorListener *parser_error_listener;

  if (unlikely(!parser)) {
    parser = new GQLParser(nullptr);
    __lsan_ignore_object(parser);

    parser_error_listener = new ParserErrorListener();
    __lsan_ignore_object(parser_error_listener);

    parser->removeErrorListeners();
    parser->addErrorListener(parser_error_listener);

    std::shared_ptr<DefaultErrorStrategy> default_handler = std::make_shared<DefaultErrorStrategy>();
    parser->setErrorHandler(default_handler);
    parser->getInterpreter<atn::ParserATNSimulator>()->setPredictionMode(atn::PredictionMode::LL);
  }

  parser->setInputStream(input);
  return parser;
}

GQLParser *GQLParserUtils::getParserSLL(IntStream *input) {
  static thread_local GQLParser *parser;
  static thread_local ParserErrorListener *parser_error_listener;

  if (unlikely(!parser)) {
    parser = new GQLParser(nullptr);
    __lsan_ignore_object(parser);

    parser_error_listener = new ParserErrorListener();
    __lsan_ignore_object(parser_error_listener);

    parser->removeErrorListeners();
    parser->addErrorListener(parser_error_listener);

    std::shared_ptr<BailErrorStrategy> bail_error_handler = std::make_shared<BailErrorStrategy>();
    parser->setErrorHandler(bail_error_handler);
    parser->getInterpreter<atn::ParserATNSimulator>()->setPredictionMode(atn::PredictionMode::SLL);
  }

  parser->setInputStream(input);
  return parser;
}

void GQLParserUtils::parseProgram(std::string_view query) { parseWithFallback(query, &GQLParser::gqlProgram); }

}  // namespace OPENGQL

}  // namespace DB
