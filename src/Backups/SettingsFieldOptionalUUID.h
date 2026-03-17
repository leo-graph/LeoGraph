#pragma once

#include <Core/SettingsFields.h>
#include <optional>

namespace DB {
struct SettingFieldOptionalUUID {
  std::optional<UUID> value;

  explicit SettingFieldOptionalUUID(const std::optional<UUID>& value_) : value(value_) {}

  explicit SettingFieldOptionalUUID(const Field& field);

  explicit operator Field() const;
};
}  // namespace DB
