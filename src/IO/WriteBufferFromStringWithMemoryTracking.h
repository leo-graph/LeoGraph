#pragma once

#include <Common/StringWithMemoryTracking.h>
#include <IO/WriteBufferFromVector.h>

namespace DB {

class WriteBufferFromStringWithMemoryTracking final : public WriteBufferFromVectorImpl<StringWithMemoryTracking> {
  using Base = WriteBufferFromVectorImpl;

 public:
  explicit WriteBufferFromStringWithMemoryTracking(StringWithMemoryTracking& vector_) : Base(vector_) {}

  WriteBufferFromStringWithMemoryTracking(StringWithMemoryTracking& vector_, AppendModeTag tag_) : Base(vector_, tag_) {}
  ~WriteBufferFromStringWithMemoryTracking() override;
};

}  // namespace DB
