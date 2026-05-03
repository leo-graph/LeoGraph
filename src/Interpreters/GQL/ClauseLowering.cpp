#include <Interpreters/GQL/ClauseLowering.h>

#include <charconv>
#include <limits>
#include <unordered_set>

#include <Core/SortDescription.h>
#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Interpreters/GQL/ExpressionLowering.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/FilterStep.h>
#include <Processors/QueryPlan/LimitStep.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Processors/QueryPlan/SortingStep.h>
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

int getNullsDirection(const GAST::GQLOrderByItem & item, int direction)
{
    if (item.null_ordering == GAST::GQLOrderByItem::NullOrdering::NullsFirst)
        return -direction;
    if (item.null_ordering == GAST::GQLOrderByItem::NullOrdering::NullsLast)
        return direction;

    return 1;
}

String getOrderByColumnName(const GAST::GQLOrderByItem & item, const Block & header)
{
    const auto * expr = item.expression ? item.expression->as<GAST::GQLExpr>() : nullptr;
    if (!expr || expr->kind != GAST::GQLExpr::Kind::Identifier)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY currently supports only projected column identifiers");

    if (!header.has(expr->text))
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY column '{}' is not available in current plan header", expr->text);

    return expr->text;
}

UInt64 getSortLimit(const GAST::GQLPageClause & page, UInt64 limit, UInt64 offset)
{
    if (!page.limit)
        return 0;
    if (limit > std::numeric_limits<UInt64>::max() - offset)
        return 0;

    return limit + offset;
}

void lowerOrderByClause(QueryPlan & plan, const GAST::GQLOrderByClause & order_by, UInt64 sort_limit, ContextPtr context)
{
    if (order_by.items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY must contain at least one sort item");

    auto current_header = plan.getCurrentHeader();
    SortDescription description;
    description.reserve(order_by.items.size());
    for (const auto & item_ast : order_by.items)
    {
        const auto * item = item_ast ? item_ast->as<GAST::GQLOrderByItem>() : nullptr;
        if (!item)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY item must be GQLOrderByItem");

        const int direction = item->descending ? -1 : 1;
        description.emplace_back(getOrderByColumnName(*item, *current_header), direction, getNullsDirection(*item, direction));
    }

    plan.addStep(std::make_unique<SortingStep>(
        current_header,
        std::move(description),
        sort_limit,
        SortingStep::Settings(context->getSettingsRef())));
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

void lowerSelectClause(
    QueryPlan & plan,
    const GAST::GQLSelectClause & select,
    ContextPtr context,
    PlanScope & scope,
    bool source_was_lowered)
{
    if (select.distinct)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT DISTINCT is not supported");
    if (select.group_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT GROUP BY is not supported");
    if (select.having)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT HAVING is not supported");
    if (select.source && !source_was_lowered)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT FROM source must be lowered before SELECT projection");

    if (select.where)
    {
        if (!source_was_lowered)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT WHERE is not supported without source lowering");

        const auto * where = select.where->as<GAST::GQLWhereClause>();
        if (!where)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT WHERE node must be GQLWhereClause");

        lowerWhereClause(plan, *where, context, "SELECT", scope);
    }

    if (select.select_all)
    {
        if (!source_was_lowered)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "SELECT * is not supported without a source");

        return;
    }

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

void lowerLetClause(QueryPlan & plan, const GAST::GQLLetClause & let, ContextPtr context, PlanScope & scope)
{
    if (let.items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "LET must contain at least one assignment");

    auto current_header = plan.getCurrentHeader();
    ActionsDAG dag(current_header->getColumnsWithTypeAndName());
    ActionsDAG::NodeRawConstPtrs outputs = dag.getOutputs();
    std::unordered_set<String> assigned_names;

    for (const auto & item_ast : let.items)
    {
        const auto * item = item_ast ? item_ast->as<GAST::GQLAssignmentItem>() : nullptr;
        if (!item)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL LET item must be GQLAssignmentItem");
        if (item->name.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LET assignment name must be non-empty");
        if (item->type)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Typed GQL LET assignments are not supported");
        if (item->value_keyword)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "LET VALUE assignments are not supported");
        if (!item->value)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LET assignment '{}' must have a value", item->name);
        if (scope.hasBinding(item->name))
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LET assignment '{}' conflicts with an existing binding", item->name);
        if (!assigned_names.insert(item->name).second)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Duplicate GQL LET assignment '{}'", item->name);

        const auto & value = lowerExpression(*item->value, dag, context, scope);
        outputs.push_back(&dag.addAlias(value, item->name));
    }

    dag.getOutputs() = std::move(outputs);
    plan.addStep(std::make_unique<ExpressionStep>(current_header, std::move(dag)));
    scope.replaceWithHeader(*plan.getCurrentHeader(), BindingKind::Projection);
}

void lowerPageClause(QueryPlan & plan, const GAST::GQLPageClause & page, ContextPtr context)
{
    UInt64 offset = 0;
    if (page.offset)
    {
        const auto * offset_expr = page.offset->as<GAST::GQLExpr>();
        if (!offset_expr)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL OFFSET must be a GQL expression");

        offset = extractUnsignedIntegerLiteral(*offset_expr, "OFFSET");
    }

    UInt64 limit = std::numeric_limits<UInt64>::max();
    if (page.limit)
    {
        const auto * limit_expr = page.limit->as<GAST::GQLExpr>();
        if (!limit_expr)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LIMIT must be a GQL expression");

        limit = extractUnsignedIntegerLiteral(*limit_expr, "LIMIT");
    }

    if (!page.order_by && !page.offset && !page.limit)
    {
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL page clause without OFFSET or LIMIT is not supported");
    }

    if (page.order_by)
    {
        const auto * order_by = page.order_by->as<GAST::GQLOrderByClause>();
        if (!order_by)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY node must be GQLOrderByClause");

        lowerOrderByClause(plan, *order_by, getSortLimit(page, limit, offset), context);
    }

    if (page.offset || page.limit)
        plan.addStep(std::make_unique<LimitStep>(plan.getCurrentHeader(), static_cast<size_t>(limit), static_cast<size_t>(offset)));
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
        lowerPageClause(plan, *page, context);
        return;
    }

    if (const auto * where = clause_ast->as<GAST::GQLWhereClause>())
    {
        lowerWhereClause(plan, *where, context, "GQL FILTER", scope);
        return;
    }

    if (const auto * let = clause_ast->as<GAST::GQLLetClause>())
    {
        lowerLetClause(plan, *let, context, scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL clause: {}", clause_ast->getID(' '));
}

}

}
