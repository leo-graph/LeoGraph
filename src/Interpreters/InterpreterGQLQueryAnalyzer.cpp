#include <Interpreters/InterpreterGQLQueryAnalyzer.h>

#include <Analyzer/GQL/GQLQueryTreeBuilder.h>
#include <Interpreters/Context.h>
#include <Interpreters/GQL/GQLPlanner.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/Optimizations/QueryPlanOptimizationSettings.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <QueryPipeline/QueryPipelineBuilder.h>
#include <Common/Exception.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace
{

/** Build GQL QueryTree and run analysis passes.
 *
 * This is analogous to buildQueryTreeAndRunPasses() for SQL queries.
 * Currently, it only builds the QueryTree; analysis passes will be
 * added in a future step.
 */
QueryTreeNodePtr buildGQLQueryTreeAndRunPasses(
    const ASTPtr & query, const ContextPtr & context, const GQL::PlanEnvironment & environment)
{
    if (!query)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL query AST is null");

    // Step 1: Build QueryTree from Parser AST
    auto query_tree = GQL::buildGQLQueryTree(*query, context);

    // Step 2: Run analysis passes
    // TODO: Implement GQLQueryTreePassManager
    // GQL::GQLQueryTreePassManager pass_manager(context);
    // pass_manager.run(query_tree);

    return query_tree;
}

} // anonymous namespace

InterpreterGQLQueryAnalyzer::InterpreterGQLQueryAnalyzer(
    const ASTPtr & query_, const ContextPtr & context_, const GQL::PlanEnvironment & environment_)
    : query(query_)
    , context(context_)
    , environment(environment_)
    , query_tree(buildGQLQueryTreeAndRunPasses(query, context, environment))
{
}

InterpreterGQLQueryAnalyzer::InterpreterGQLQueryAnalyzer(
    const QueryTreeNodePtr & query_tree_, const ContextPtr & context_, const GQL::PlanEnvironment & environment_)
    : query(nullptr) // Will be set when toASTImpl is implemented
    , context(context_)
    , environment(environment_)
    , query_tree(query_tree_)
{
    if (!query_tree)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL QueryTree is null");

    // TODO: Implement toASTImpl() for GQL QueryTree nodes
    // query = query_tree->toAST();
}

BlockIO InterpreterGQLQueryAnalyzer::execute()
{
    BlockIO result;
    QueryPlan query_plan;

    buildQueryPlan(query_plan);

    auto builder = query_plan.buildQueryPipeline(
        QueryPlanOptimizationSettings(context), BuildQueryPipelineSettings(context));
    result.pipeline = QueryPipelineBuilder::getPipeline(std::move(*builder));

    return result;
}

void InterpreterGQLQueryAnalyzer::buildQueryPlan(QueryPlan & query_plan)
{
    if (!query_tree)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL QueryTree is null");

    // Convert QueryTree to QueryPlan
    // TODO: Modify GQLPlanner to accept QueryTreeNodePtr instead of IAST
    // For now, we need to convert back to AST (temporary solution)

    if (!query)
    {
        // If we don't have the original AST, we need to reconstruct it
        // This is a temporary workaround until we modify GQLPlanner
        throw Exception(
            ErrorCodes::NOT_IMPLEMENTED,
            "Converting GQL QueryTree back to AST is not yet implemented. "
            "GQLPlanner needs to be modified to accept QueryTreeNodePtr directly.");
    }

    // Temporary: use the old path through AST
    GQL::buildGQLQueryPlan(query_plan, *query, context, environment);
    query_plan.addInterpreterContext(context);
}

SharedHeader InterpreterGQLQueryAnalyzer::getSampleBlock() const
{
    // TODO: Extract header from QueryTree without executing
    // For now, we need to build the plan to get the header
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "InterpreterGQLQueryAnalyzer::getSampleBlock is not yet implemented");
}

}
