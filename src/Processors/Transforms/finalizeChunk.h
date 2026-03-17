#pragma once

#include <Interpreters/AggregateDescription.h>
#include <Processors/Chunk.h>
#include <vector>

namespace DB {

class Block;
using ColumnsMask = std::vector<bool>;

ColumnsMask getAggregatesMask(const Block& header, const AggregateDescriptions& aggregates);

/// Convert ColumnAggregateFunction to real values.
///
/// @param aggregates_mask columns to convert (see getAggregatesMask())
void finalizeChunk(Chunk& chunk, const ColumnsMask& aggregates_mask);

}  // namespace DB
