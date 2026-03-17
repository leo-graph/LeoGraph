#pragma once
#include "config.h"

#include <base/types.h>
#include <optional>

namespace DB::Iceberg {
struct PositionDeleteObject {
  String file_path;
  String file_format;
  std::optional<String>
      reference_data_file_path;  // now it is always std::nullopt. Exists for compatibility reasons of the iceberg cluster function.
};
}  // namespace DB::Iceberg
