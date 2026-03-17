#pragma once

#include <Common/LoggingFormatStringHelpers.h>
#include <functional>

namespace DB {

void assertProcessUserMatchesDataOwner(const std::string &path, std::function<void(const PreformattedMessage &)> on_warning);

}
