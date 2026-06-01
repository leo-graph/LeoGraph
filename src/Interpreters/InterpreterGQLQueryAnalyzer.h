#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>
#include <Analyzer/IQueryTreeNode.h>
#include <QueryPipeline/BlockIO.h>

namespace DB
{

class QueryPlan;

/** Interpreter for GQL queries using QueryTree-based analysis.
 *
 * This is the GQL counterpart of InterpreterSelectQueryAnalyzer.
 * It follows the standard ClickHouse query processing pipeline:
 *
 * 1. Parser AST -> QueryTree (via GQLQueryTreeBuilder)
 * 2. QueryTree analysis and optimization (via GQLQueryTreePassManager)
 * 3. QueryTree -> QueryPlan (via GQLPlanner)
 * 4. QueryPlan -> QueryPipeline -> Execution
 *
 * Example usage:
 *   ASTPtr ast = ParserGQLQuery::parse("MATCH (n) RETURN n");
 *   InterpreterGQLQueryAnalyzer interpreter(ast, context);
 *   BlockIO result = interpreter.execute();
 *
 * This interpreter replaces the old InterpreterGQLQuery which directly
 * converted Parser AST to QueryPlan without the QueryTree intermediate layer.
 */
class InterpreterGQLQueryAnalyzer
{
public:
    /** Construct from Parser AST.
     *
     * This is the primary constructor for top-level GQL queries.
     * It will build the QueryTree and run analysis passes.
     */
    InterpreterGQLQueryAnalyzer(
        const ASTPtr & query_,
        const ContextPtr & context_);

    /** Construct from QueryTree.
     *
     * This constructor is used for nested queries or when the QueryTree
     * has already been built and analyzed externally.
     */
    InterpreterGQLQueryAnalyzer(
        const QueryTreeNodePtr & query_tree_,
        const ContextPtr & context_);

    /** Execute the query and return the result pipeline. */
    BlockIO execute();

    /** Get the sample block (header) without executing the query. */
    SharedHeader getSampleBlock() const;

    /** Get the QueryTree for inspection or further processing. */
    QueryTreeNodePtr getQueryTree() const { return query_tree; }

    /** Build the QueryPlan without executing it. */
    void buildQueryPlan(QueryPlan & query_plan);

private:
    ASTPtr query;
    ContextPtr context;
    QueryTreeNodePtr query_tree;  // GQLLinearQueryNode or GQLCombinedQueryNode
};

}
