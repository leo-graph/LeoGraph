#pragma once
#include <Core/Block.h>
#include <Storages/MergeTree/MergeTreePartition.h>
#include <memory>

namespace DB {

struct BlockWithPartition {
  std::shared_ptr<Block> block;
  MergeTreePartition partition;
  std::string partition_id;

  BlockWithPartition() = default;
  BlockWithPartition(const BlockWithPartition& block_) = default;
  BlockWithPartition(BlockWithPartition&& block_) = default;

  BlockWithPartition(std::shared_ptr<Block> block_, Row partition_);

  BlockWithPartition& operator=(BlockWithPartition&& other) = default;
};

}  // namespace DB
