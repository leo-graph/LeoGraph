#include <Core/Block.h>
#include <Core/BlockNameMap.h>

namespace DB {

BlockNameMap getNamesToIndexesMap(const Block& block) {
  auto const& index_by_name = block.getIndexByName();

  BlockNameMap res(index_by_name.size());
  res.set_empty_key(std::string_view{});
  for (const auto& [name, index] : index_by_name) res[name] = index;
  return res;
}

}  // namespace DB
