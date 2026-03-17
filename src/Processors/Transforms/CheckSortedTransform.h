#pragma once
#include <Columns/IColumn_fwd.h>
#include <Core/SortDescription.h>
#include <Processors/ISimpleTransform.h>

namespace DB {
/// Streams checks that flow of blocks is sorted in the sort_description order
/// Othrewise throws exception in readImpl function.
class CheckSortedTransform : public ISimpleTransform {
 public:
  CheckSortedTransform(SharedHeader header, const SortDescription& sort_description);

  String getName() const override { return "CheckSortedTransform"; }
  void setDescription(const String& str) { description = str; }

 protected:
  void transform(Chunk& chunk) override;

 private:
  SortDescriptionWithPositions sort_description_map;
  Columns last_row;
  String description;
  size_t chunk_num = 0;
  size_t rows_read = 0;
};
}  // namespace DB
