//
// Parser.cpp
//
// Library: JSON
// Package: JSON
// Module:  Parser
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//

#include "Poco/JSON/Parser.h"
#include "Poco/Ascii.h"
#include "Poco/JSON/JSONException.h"
#include "Poco/String.h"
#include "Poco/Token.h"
#include "Poco/UTF8Encoding.h"
#undef min
#undef max
#include <clocale>
#include <istream>
#include <limits>

namespace Poco {
namespace JSON {

Parser::Parser(const Handler::Ptr& pHandler, std::size_t bufSize) : ParserImpl(pHandler, bufSize) {}

Parser::~Parser() {}

void Parser::setHandler(const Handler::Ptr& pHandler) { setHandlerImpl(pHandler); }

}  // namespace JSON
}  // namespace Poco
