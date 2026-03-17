#include <Access/AccessChangesNotifier.h>
#include <Access/ReplicatedAccessStorage.h>
#include <gtest/gtest.h>

using namespace DB;

namespace DB {
namespace ErrorCodes {
extern const int NO_ZOOKEEPER;
}
}  // namespace DB

TEST(ReplicatedAccessStorage, ShutdownWithFailedStartup) {
  auto get_zk = []() { return std::shared_ptr<zkutil::ZooKeeper>(); };

  AccessChangesNotifier changes_notifier;

  try {
    auto storage = ReplicatedAccessStorage("replicated", "/clickhouse/access", get_zk, changes_notifier, false, false);
  } catch (Exception& e) {
    if (e.code() != ErrorCodes::NO_ZOOKEEPER) throw;
  }
}
