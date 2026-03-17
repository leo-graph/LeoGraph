#pragma once

#include <Common/Logger.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <boost/core/noncopyable.hpp>
#include <string>

namespace DB {

namespace PlacementInfo {

static constexpr auto PLACEMENT_CONFIG_PREFIX = "placement";
static constexpr auto DEFAULT_AZ_FILE_PATH = "/run/instance-metadata/node-zone";
static constexpr auto DEFAULT_REGION_FILE_PATH = "/run/instance-metadata/node-region";

/// A singleton providing information on where in cloud server is running.
class PlacementInfo : private boost::noncopyable {
 public:
  static PlacementInfo& instance();

  void initialize(const Poco::Util::AbstractConfiguration& config);

  std::string getAvailabilityZone() const;
  std::string getRegion() const;

 private:
  PlacementInfo() = default;

  LoggerPtr log = getLogger("CloudPlacementInfo");

  bool initialized;

  bool use_imds;
  std::string availability_zone;
  std::string region;
};

}  // namespace PlacementInfo
}  // namespace DB
