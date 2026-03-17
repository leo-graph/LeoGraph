#include <Common/NamePrompter.h>
#include <IO/WriteHelpers.h>

namespace DB {

String getHintsErrorMessageSuffix(const std::vector<String>& hints) {
  if (hints.empty()) return {};

  return ". Maybe you meant: " + toString(hints);
}

void appendHintsMessage(String& message, const std::vector<String>& hints) { message += getHintsErrorMessageSuffix(hints); }

}  // namespace DB
