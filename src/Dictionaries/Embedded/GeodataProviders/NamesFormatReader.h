#pragma once

#include <Dictionaries/Embedded/GeodataProviders/INamesProvider.h>
#include <IO/ReadBuffer.h>

namespace DB {

// Reads regions names list in geoexport format
class LanguageRegionsNamesFormatReader : public ILanguageRegionsNamesReader {
 private:
  ReadBufferPtr input;

 public:
  explicit LanguageRegionsNamesFormatReader(ReadBufferPtr input_) : input(std::move(input_)) {}

  bool readNext(RegionNameEntry& entry) override;
};

}  // namespace DB
