#pragma once

#include <IO/HTTPHeaderEntries.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace DB {

class HTTPHeaderFilter {
 public:
  void setValuesFromConfig(const Poco::Util::AbstractConfiguration& config);
  void checkAndNormalizeHeaders(HTTPHeaderEntries& entries) const;

 private:
  std::unordered_set<std::string> forbidden_headers;
  std::vector<std::string> forbidden_headers_regexp;

  mutable std::mutex mutex;
};

}  // namespace DB
