#pragma once

#include <Analyzer/IQueryTreePass.h>
#include <Interpreters/Context_fwd.h>

namespace DB::GQL
{

/** Pass manager for GQL QueryTree passes.
  *
  * This mirrors the SQL `QueryTreePassManager` but stays deliberately isolated from
  * the SQL pass pipeline and its validation: GQL QueryTree roots
  * (`GQLLinearQueryNode`, `GQLCombinedQueryNode`, ...) are not SQL `QueryNode`-shaped,
  * so SQL `QueryAnalysisPass` and the SQL pass validator must not run over them.
  *
  * Passes are executed in insertion order. `addDefaultPasses` installs the standard
  * GQL analysis pipeline.
  */
class GQLQueryTreePassManager
{
public:
    explicit GQLQueryTreePassManager(ContextPtr context_);

    void addPass(QueryTreePassPtr pass);

    void run(QueryTreeNodePtr & query_tree_node);

    static void addDefaultPasses(GQLQueryTreePassManager & manager);

private:
    ContextPtr context;
    QueryTreePasses passes;
};

}
