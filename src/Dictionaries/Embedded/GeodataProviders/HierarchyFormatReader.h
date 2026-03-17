#pragma once

#include <Dictionaries/Embedded/GeodataProviders/IHierarchiesProvider.h>
#include <IO/ReadBuffer.h>

namespace DB {

// Reads regions hierarchy in geoexport format
class RegionsHierarchyFormatReader : public IRegionsHierarchyReader {
 private:
  ReadBufferPtr input;

 public:
  explicit RegionsHierarchyFormatReader(ReadBufferPtr input_) : input(std::move(input_)) {}

  bool readNext(RegionEntry& entry) override;
};

}  // namespace DB
