#pragma once

#include <Common/Logger.h>
#include <Poco/Net/SocketAddress.h>

namespace Poco {
class Logger;
}

namespace DB {

Poco::Net::SocketAddress makeSocketAddress(const std::string& host, uint16_t port, Poco::Logger* log);

Poco::Net::SocketAddress makeSocketAddress(const std::string& host, uint16_t port, LoggerPtr log);

}  // namespace DB
