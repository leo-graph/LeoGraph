#include <base/getFQDNOrHostName.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/NetException.h>

namespace {
std::string getFQDNOrHostNameImpl() {
#if defined(OS_DARWIN) || defined(OS_SUNOS)
  return Poco::Net::DNS::hostName();
#else
  try {
    return Poco::Net::DNS::thisHost().name();
  } catch (const Poco::Net::NetException &) {
    return Poco::Net::DNS::hostName();
  }
#endif
}
}  // namespace

const std::string& getFQDNOrHostName() {
  static std::string result = getFQDNOrHostNameImpl();
  return result;
}
