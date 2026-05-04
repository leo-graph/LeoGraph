#include <Interpreters/GQL/SourceCompositionLowering.h>

#include <Interpreters/GQL/MatchLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Common/Exception.h>

#include <utility>
#include <vector>

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

ASTPtr cloneOrNull(const ASTPtr & ast)
{
    return ast ? ast->clone() : nullptr;
}

bool sameGraphReference(const IAST & lhs_ast, const IAST & rhs_ast)
{
    const auto * lhs = lhs_ast.as<GAST::GQLGraphExpression>();
    const auto * rhs = rhs_ast.as<GAST::GQLGraphExpression>();
    if (!lhs || !rhs)
        return false;

    if (lhs->kind != rhs->kind || lhs->text != rhs->text)
        return false;

    return !lhs->value && !rhs->value;
}

void lowerSameGraphMatchSourceList(
    QueryPlan & plan,
    const GAST::GQLSelectSourceList & source_list,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    std::vector<const GAST::GQLMatchClause *> matches;
    matches.reserve(source_list.items.size());

    const ASTPtr * graph_reference = nullptr;
    for (const auto & item_ast : source_list.items)
    {
        const auto * item = item_ast ? item_ast->as<GAST::GQLSelectSourceItem>() : nullptr;
        if (!item)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list item must be GQLSelectSourceItem");
        if (!item->graph_reference)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM graph match source must have a graph reference");

        if (!graph_reference)
        {
            graph_reference = &item->graph_reference;
        }
        else if (!sameGraphReference(**graph_reference, *item->graph_reference))
        {
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists with different graph references require source composition");
        }

        const auto * match = item->source ? item->source->as<GAST::GQLMatchClause>() : nullptr;
        if (!match)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists only support graph MATCH sources");

        matches.push_back(match);
    }

    auto previous_graph = cloneOrNull(scope.getActiveGraph());
    PlanScope source_scope = scope;
    source_scope.setActiveGraph((*graph_reference)->clone());

    lowerMatchClauseSequence(plan, matches, context, environment, source_scope);

    scope = std::move(source_scope);
    scope.setActiveGraph(std::move(previous_graph));
}

}

void lowerSelectSourceList(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSelectSourceList & source_list,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    if (source_list.items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list must contain at least one source");

    lowerSameGraphMatchSourceList(plan, source_list, context, environment, scope);
}

}
