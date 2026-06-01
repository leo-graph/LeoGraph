#include <Analyzer/GQL/Passes/GQLQueryTreePassManager.h>

#include <Analyzer/GQL/Passes/GQLNameResolutionPass.h>

#include <utility>

namespace DB::GQL
{

GQLQueryTreePassManager::GQLQueryTreePassManager(ContextPtr context_) : context(std::move(context_))
{
}

void GQLQueryTreePassManager::addPass(QueryTreePassPtr pass)
{
    passes.push_back(std::move(pass));
}

void GQLQueryTreePassManager::run(QueryTreeNodePtr & query_tree_node)
{
    for (auto & pass : passes)
        pass->run(query_tree_node, context);
}

void GQLQueryTreePassManager::addDefaultPasses(GQLQueryTreePassManager & manager)
{
    manager.addPass(std::make_unique<GQLNameResolutionPass>());
}

}
