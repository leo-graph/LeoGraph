#pragma once

#include <memory>

namespace Poco {
class Logger;
using LoggerPtr = std::shared_ptr<Logger>;
}  // namespace Poco

using LoggerPtr = std::shared_ptr<Poco::Logger>;
