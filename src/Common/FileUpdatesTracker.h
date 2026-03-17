#pragma once

#include <Common/filesystemHelpers.h>
#include <Poco/Timestamp.h>
#include <filesystem>
#include <string>

class FileUpdatesTracker {
 private:
  std::string path;
  Poco::Timestamp known_time;

 public:
  explicit FileUpdatesTracker(const std::string& path_) : path(path_), known_time(0) {}

  bool isModified() const { return getLastModificationTime() > known_time; }

  void fixCurrentVersion() { known_time = getLastModificationTime(); }

 private:
  Poco::Timestamp getLastModificationTime() const { return FS::getModificationTimestamp(path); }
};
