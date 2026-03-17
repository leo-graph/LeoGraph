#pragma once

#include <base/extended_types.h>
#include <base/strong_typedef.h>

namespace DB {
using UUID = StrongTypedef<UInt128, struct UUIDTag>;
}
