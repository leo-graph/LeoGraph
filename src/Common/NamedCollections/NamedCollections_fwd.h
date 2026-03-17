#pragma once
#include <map>
#include <memory>

namespace DB {

class NamedCollection;
using NamedCollectionPtr = std::shared_ptr<const NamedCollection>;
using MutableNamedCollectionPtr = std::shared_ptr<NamedCollection>;
using NamedCollectionsMap = std::map<std::string, MutableNamedCollectionPtr>;

}  // namespace DB
