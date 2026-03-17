#include <Columns/ColumnsNumber.h>
#include <Common/logger_useful.h>
#include <Common/SipHash.h>
#include <IO/WriteHelpers.h>
#include <Storages/MergeTree/InsertBlockInfo.h>
#include <filesystem>

namespace DB {

BlockWithPartition::BlockWithPartition(std::shared_ptr<Block> block_, Row partition_)
    : block(std::move(block_)), partition(std::move(partition_)) {}

}  // namespace DB
