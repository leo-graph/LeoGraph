#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace DB {

/// https://specifications.freedesktop.org/basedir-spec/latest/
class XDGBaseDirectories {
 public:
  static fs::path getConfigurationHome();
  static fs::path getDataHome();
  static fs::path getStateHome();
  static fs::path getCacheHome();
};

}  // namespace DB
