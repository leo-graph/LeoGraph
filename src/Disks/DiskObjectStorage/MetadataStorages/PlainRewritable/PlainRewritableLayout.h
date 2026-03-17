#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace DB {

class PlainRewritableLayout {
 public:
  constexpr static std::string PREFIX_PATH_FILE_NAME = "prefix.path";
  constexpr static std::string METADATA_DIRECTORY_TOKEN = "__meta";
  constexpr static std::string ROOT_DIRECTORY_TOKEN = "__root";

  explicit PlainRewritableLayout(std::string object_storage_common_key_prefix_);

  std::string constructMetadataDirectoryKey() const;
  std::string constructRootFilesDirectoryKey() const;
  std::string constructFilesDirectoryKey(const std::string& directory_remote_path) const;
  std::string constructFileObjectKey(const std::string& directory_remote_path, const std::string& file_name) const;
  std::string constructDirectoryObjectKey(const std::string& directory_remote_path) const;

  std::optional<std::pair<std::string, std::string>> parseFileObjectKey(const std::string& key) const;
  std::optional<std::string> parseDirectoryObjectKey(const std::string& key) const;

 private:
  const std::filesystem::path object_storage_common_key_prefix;
};

}  // namespace DB
