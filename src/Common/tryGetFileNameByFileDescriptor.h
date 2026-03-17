#pragma once

#include <base/types.h>
#include <optional>

namespace DB {
/// Supports only Linux/MacOS. On other platforms, returns nullopt.
std::optional<String> tryGetFileNameFromFileDescriptor(int fd);
}  // namespace DB
