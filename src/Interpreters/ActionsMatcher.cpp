#include <Interpreters/ActionsMatcher.h>
#include <IO/WriteHelpers.h>

namespace DB {

String ActionsMatcher::Data::getUniqueName(const String& prefix) {
  auto result = prefix;

  // First, try the name without any suffix, because it is currently
  // used both as a display name and a column id.
  while (hasColumn(result)) {
    result = prefix + "_" + toString(next_unique_suffix);
    ++next_unique_suffix;
  }

  return result;
}

}  // namespace DB
