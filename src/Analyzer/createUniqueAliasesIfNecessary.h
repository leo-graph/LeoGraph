#pragma once

#include <Interpreters/Context_fwd.h>
#include <memory>

class IQueryTreeNode;
using QueryTreeNodePtr = std::shared_ptr<IQueryTreeNode>;

namespace DB {

void createUniqueAliasesIfNecessary(QueryTreeNodePtr& node, const ContextPtr& context);

}
