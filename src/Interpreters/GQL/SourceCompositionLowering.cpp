#include <Interpreters/GQL/SourceCompositionLowering.h>

#include <Interpreters/GQL/MatchLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Interpreters/GQL/SourceLowering.h>
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

struct SelectSourceListEntry
{
    ASTPtr graph_reference;
    SourceClauseKind source_kind = SourceClauseKind::None;
    const GAST::GQLMatchClause * match = nullptr;
};

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

SelectSourceListEntry classifySourceListEntry(const ASTPtr & item_ast)
{
    const auto * item = item_ast ? item_ast->as<GAST::GQLSelectSourceItem>() : nullptr;
    if (!item)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source list item must be GQLSelectSourceItem");
    if (!item->graph_reference)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM graph match source must have a graph reference");

    SelectSourceListEntry result;
    result.graph_reference = item->graph_reference;
    result.source_kind = classifySourceIntroducingClause(item->source);
    result.match = item->source ? item->source->as<GAST::GQLMatchClause>() : nullptr;
    return result;
}

std::vector<SelectSourceListEntry> classifySourceListEntries(const GAST::GQLSelectSourceList & source_list)
{
    std::vector<SelectSourceListEntry> entries;
    entries.reserve(source_list.items.size());
    for (const auto & item_ast : source_list.items)
        entries.push_back(classifySourceListEntry(item_ast));

    return entries;
}

bool isSameGraphMatchSourceList(const std::vector<SelectSourceListEntry> & entries)
{
    if (entries.empty())
        return false;

    const auto & graph_reference = entries.front().graph_reference;
    for (const auto & entry : entries)
    {
        if (entry.source_kind != SourceClauseKind::Match || !entry.match)
            return false;
        if (!sameGraphReference(*graph_reference, *entry.graph_reference))
            return false;
    }

    return true;
}

bool hasDifferentGraphReferences(const std::vector<SelectSourceListEntry> & entries)
{
    if (entries.empty())
        return false;

    const auto & graph_reference = entries.front().graph_reference;
    for (const auto & entry : entries)
    {
        if (!sameGraphReference(*graph_reference, *entry.graph_reference))
            return true;
    }

    return false;
}

void lowerSameGraphMatchSourceList(
    QueryPlan & plan,
    const std::vector<SelectSourceListEntry> & entries,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    std::vector<const GAST::GQLMatchClause *> matches;
    matches.reserve(entries.size());
    for (const auto & entry : entries)
        matches.push_back(entry.match);

    auto previous_graph = cloneOrNull(scope.getActiveGraph());
    PlanScope source_scope = scope;
    source_scope.setActiveGraph(entries.front().graph_reference->clone());

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

    const auto entries = classifySourceListEntries(source_list);
    if (isSameGraphMatchSourceList(entries))
    {
        lowerSameGraphMatchSourceList(plan, entries, context, environment, scope);
        return;
    }

    if (hasDifferentGraphReferences(entries))
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists with different graph references require source composition");

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lists only support graph MATCH sources");
}

}
