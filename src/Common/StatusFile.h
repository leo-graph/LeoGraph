#pragma once

#include <boost/noncopyable.hpp>
#include <functional>
#include <string>

namespace DB {

class WriteBuffer;

/** Provides that no more than one server works with one data directory.
 */
class StatusFile : private boost::noncopyable {
 public:
  using FillFunction = std::function<void(WriteBuffer &)>;

  StatusFile(std::string path_, FillFunction fill);
  ~StatusFile();

  /// You can use one of these functions to fill the file or provide your own.
  static FillFunction write_pid;
  static FillFunction write_full_info;

 private:
  const std::string path;
  int fd = -1;
};

}  // namespace DB
