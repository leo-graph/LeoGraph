#include <Interpreters/GQL/ClauseLowering.h>

#include <charconv>
#include <limits>

#include <Interpreters/ActionsDAG.h>
#include <Interpreters/GQL/ExpressionLowering.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/FilterStep.h>
#include <Processors/QueryPlan/LimitStep.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Common/Exception.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

void lowerWherePredicate(QueryPlan & plan, const GAST::GQLExpr & predicate, ContextPtr context, const PlanScope & scope)
{
    auto current_header = plan.getCurrentHeader();
    ActionsDAG dag(current_header->getColumnsWithTypeAndName());

    const auto & filter_node = lowerExpression(predicate, dag, context, scope);
    dag.getOutputs().push_back(&filter_node);

    plan.addStep(std::make_unique<FilterStep>(current_header, std::move(dag), filter_node.result_name, true));
}

UInt64 extractUnsignedIntegerLiteral(const GAST::GQLExpr & literal, std::string_view context_name)
{
    if (literal.kind != GAST::GQLExpr::Kind::Literal || literal.text.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only unsigned integer literals are supported for GQL {}", context_name);

    UInt64 value = 0;
    const char * begin = literal.text.data();
    const char * end = begin + literal.text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only unsigned integer literals are supported for GQL {}", context_name);

    return value;
}

}

void lowerWhereClause(
    QueryPlan & plan,
    const GAST::GQLWhereClause & where,
    ContextPtr context,
    std::string_view context_name,
    const PlanScope & scope)
{
    if (where.type != GAST::GQLWhereClause::Type::Where)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "HAVING inside {} is not supported", context_name);

    const auto * predicate = where.expression ? where.expression->as<GAST::GQLExpr>() : nullptr;
    if (!predicate)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} predicate must be a GQL expression", context_name);

    lowerWherePredicate(plan, *predicate, context, scope);
}

void lowerReturnClause(QueryPlan & plan, const GAST::GQLReturnClause & ret, ContextPtr context, PlanScope & scope)
{
    if (ret.distinct)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "RETURN DISTINCT is not supported");
    if (ret.group_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "RETURN GROUP BY is not supported");

    if (ret.return_all)
        return;

    lowerProjectionItems(plan, ret.items, context, "RETURN", scope);
}

void lowerSelectClause(QueryPlan & plan, const GAST::GQLSelectClause & select, ContextPtr context, PlanScope & scope)
{
    if (select.distinct)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT DISTINCT is not supported");
    if (select.select_all)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT * is not supported without a source");
    if (select.source)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source lowering is not supported");
    if (select.where)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT WHERE is not supported without source lowering");
    if (select.group_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT GROUP BY is not supported");
    if (select.having)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT HAVING is not supported");

    lowerProjectionItems(plan, select.items, context, "SELECT", scope);
}

void lowerProjectionItems(QueryPlan & plan, const ASTs & items, ContextPtr context, std::string_view context_name, PlanScope & scope)
{
    if (items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must project at least one item", context_name);

    auto current_header = plan.getCurrentHeader();
    ActionsDAG dag(current_header->getColumnsWithTypeAndName());

    ActionsDAG::NodeRawConstPtrs new_outputs;
    new_outputs.reserve(items.size());

    for (const auto & item_ast : items)
    {
        const auto * item = item_ast->as<GAST::GQLAliasedItem>();
        if (!item)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL {} item must be GQLAliasedItem", context_name);

        if (!item->expression)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL {} item must wrap an expression", context_name);

        const auto & node = lowerExpression(*item->expression, dag, context, scope);
        if (item->alias.empty())
            new_outputs.push_back(&node);
        else
            new_outputs.push_back(&dag.addAlias(node, item->alias));
    }

    dag.getOutputs() = std::move(new_outputs);
    plan.addStep(std::make_unique<ExpressionStep>(current_header, std::move(dag)));
    scope.replaceWithHeader(*plan.getCurrentHeader(), BindingKind::Projection);
}

void lowerPageClause(QueryPlan & plan, const GAST::GQLPageClause & page)
{
    if (page.order_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY is not yet supported");

    size_t offset = 0;
    if (page.offset)
    {
        const auto * offset_expr = page.offset->as<GAST::GQLExpr>();
        if (!offset_expr)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL OFFSET must be a GQL expression");

        offset = static_cast<size_t>(extractUnsignedIntegerLiteral(*offset_expr, "OFFSET"));
    }

    size_t limit = std::numeric_limits<size_t>::max();
    if (page.limit)
    {
        const auto * limit_expr = page.limit->as<GAST::GQLExpr>();
        if (!limit_expr)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LIMIT must be a GQL expression");

        limit = static_cast<size_t>(extractUnsignedIntegerLiteral(*limit_expr, "LIMIT"));
    }
    else if (!page.offset)
    {
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL page clause without OFFSET or LIMIT is not supported");
    }

    plan.addStep(std::make_unique<LimitStep>(plan.getCurrentHeader(), limit, offset));
}

void lowerPipelineClause(QueryPlan & plan, const ASTPtr & clause_ast, ContextPtr context, PlanScope & scope)
{
    if (!clause_ast)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL clause node is null");

    if (const auto * ret = clause_ast->as<GAST::GQLReturnClause>())
    {
        lowerReturnClause(plan, *ret, context, scope);
        return;
    }

    if (const auto * page = clause_ast->as<GAST::GQLPageClause>())
    {
        lowerPageClause(plan, *page);
        return;
    }

    if (const auto * where = clause_ast->as<GAST::GQLWhereClause>())
    {
        lowerWhereClause(plan, *where, context, "GQL FILTER", scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL clause: {}", clause_ast->getID(' '));
}

}

}
