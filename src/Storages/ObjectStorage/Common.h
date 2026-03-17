#pragma once

#include <Common/Exception.h>
#include <Storages/IPartitionStrategy.h>
#include <memory>

namespace DB {
struct StorageParsedArguments {
  String format = "auto";
  String compression_method = "auto";
  String structure = "auto";
  PartitionStrategyFactory::StrategyType partition_strategy_type = PartitionStrategyFactory::StrategyType::NONE;
  bool partition_columns_in_data_file = true;
  std::shared_ptr<IPartitionStrategy> partition_strategy;
};
}  // namespace DB
