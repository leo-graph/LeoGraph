#pragma once

#include <Core/SettingsEnums.h>
#include <DataTypes/DataTypesNumber.h>
#include <Interpreters/Cluster.h>
#include <Interpreters/Context_fwd.h>
#include <Interpreters/DatabaseAndTableWithAlias.h>
#include <Parsers/IAST_fwd.h>
#include <Storages/IStorage.h>

namespace DB {
struct ParallelReplicasCustomKeyFilter {
  ParallelReplicasMode filter_type;
  UInt64 range_lower;
  UInt64 range_upper;
};

/// Get AST for filter created from custom_key
/// replica_num is the number of the replica for which we are generating filter starting from 0
ASTPtr getCustomKeyFilterForParallelReplica(size_t replicas_count, size_t replica_num, ASTPtr custom_key_ast,
                                            ParallelReplicasCustomKeyFilter filter, const ColumnsDescription& columns,
                                            const ContextPtr& context);

ASTPtr parseCustomKeyForTable(const String& custom_keys, const Context& context);

}  // namespace DB
