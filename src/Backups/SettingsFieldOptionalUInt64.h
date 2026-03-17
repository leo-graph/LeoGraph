#pragma once

#include <Core/SettingsFields.h>
#include <optional>

namespace DB {

struct SettingFieldOptionalUInt64 {
  std::optional<UInt64> value;

  explicit SettingFieldOptionalUInt64(const std::optional<UInt64>& value_) : value(value_) {}

  explicit SettingFieldOptionalUInt64(const Field& field);

  explicit operator Field() const;
};

}  // namespace DB
