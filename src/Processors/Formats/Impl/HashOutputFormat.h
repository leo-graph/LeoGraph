#pragma once

#include <Common/SipHash.h>
#include <Core/Block_fwd.h>
#include <Processors/Chunk.h>
#include <Processors/Formats/IOutputFormat.h>

namespace DB {

class WriteBuffer;

/// Computes a single hash value from all columns and rows of the input.
class HashOutputFormat final : public IOutputFormat {
 public:
  HashOutputFormat(WriteBuffer& out_, SharedHeader header_);
  String getName() const override;

 private:
  void consume(Chunk chunk) override;
  void finalizeImpl() override;

  SipHash hash;
};

}  // namespace DB
