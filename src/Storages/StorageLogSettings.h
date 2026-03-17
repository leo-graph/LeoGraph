#pragma once

#include <base/types.h>
#include <memory>

namespace DB {
class ASTStorage;
class Context;
using ContextPtr = std::shared_ptr<const Context>;

String getDiskName(ASTStorage& storage_def, ContextPtr context);

struct StorageLogSettings {
  static bool hasBuiltin(std::string_view name);
};
}  // namespace DB
