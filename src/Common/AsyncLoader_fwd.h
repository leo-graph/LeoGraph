#pragma once

#include <memory>
#include <unordered_set>
#include <vector>

namespace DB {

class AsyncLoader;
class LoadTask;
using LoadTaskPtr = std::shared_ptr<LoadTask>;
using LoadTaskPtrs = std::vector<LoadTaskPtr>;

class LoadJob;
using LoadJobPtr = std::shared_ptr<LoadJob>;
using LoadJobSet = std::unordered_set<LoadJobPtr>;

}  // namespace DB
