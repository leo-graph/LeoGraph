#pragma once

#include <string>

#include <IO/BufferWithOwnMemory.h>
#include <IO/WriteBuffer.h>

namespace DB {

class WriteBufferFromFileBase : public BufferWithOwnMemory<WriteBuffer> {
 public:
  WriteBufferFromFileBase(size_t buf_size, char* existing_memory, size_t alignment);

  void sync() override = 0;
  virtual std::string getFileName() const = 0;
};

}  // namespace DB
