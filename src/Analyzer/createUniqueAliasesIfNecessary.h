#pragma once

#include <Interpreters/Context_fwd.h>
#include <memory>

namespace DB {

class IQueryTreeNode;
using QueryTreeNodePtr = std::shared_ptr<IQueryTreeNode>;

void createUniqueAliasesIfNecessary(QueryTreeNodePtr& node, const ContextPtr& context);

}
