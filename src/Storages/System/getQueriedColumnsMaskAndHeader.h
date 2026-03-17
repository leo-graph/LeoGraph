#pragma once
#include <base/types.h>
#include <Core/Names.h>

namespace DB {

class Block;

std::pair<std::vector<UInt8>, Block> getQueriedColumnsMaskAndHeader(const Block& sample_block, const Names& column_names);

}  // namespace DB
