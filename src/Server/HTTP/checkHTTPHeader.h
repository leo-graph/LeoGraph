#pragma once

#include <base/types.h>
#include <Server/HTTP/HTTPRequest.h>

namespace DB {

/// Checks that the HTTP request has a specified header with a specified value.
void checkHTTPHeader(const HTTPRequest& request, const String& header_name, const String& expected_value);

}  // namespace DB
