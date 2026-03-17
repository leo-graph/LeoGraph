#include <Common/assert_cast.h>
#include <Databases/DatabaseReplicated.h>
#include <Databases/DatabaseReplicatedHelpers.h>

namespace DB {

String getReplicatedDatabaseShardName(const DatabasePtr &database) {
  return assert_cast<const DatabaseReplicated *>(database.get())->getShardName();
}

String getReplicatedDatabaseReplicaName(const DatabasePtr &database) {
  return assert_cast<const DatabaseReplicated *>(database.get())->getReplicaName();
}

}  // namespace DB
