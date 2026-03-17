#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace DB {

using Location = std::string;
using Locations = std::vector<Location>;
using LocationSet = std::unordered_set<Location>;

}  // namespace DB
