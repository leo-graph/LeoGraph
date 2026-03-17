#pragma once

#include <optional>
#include <string>

namespace DB {

/// Return path to existing configuration file.
std::optional<std::string> getClientConfigPath(const std::string& home_path);

}  // namespace DB
