#pragma once

#include <base/types.h>
#include <Processors/ISource.h>
#include <memory>

namespace DB {

/// Source for the Distributed engine on-disk file for async INSERT.
class DistributedAsyncInsertSource : public ISource {
  struct Data;
  explicit DistributedAsyncInsertSource(std::unique_ptr<Data> data);

 public:
  explicit DistributedAsyncInsertSource(const String& file_name);
  ~DistributedAsyncInsertSource() override;
  String getName() const override { return "DistributedAsyncInsertSource"; }

 protected:
  Chunk generate() override;

 private:
  std::unique_ptr<Data> data;
};

}  // namespace DB
