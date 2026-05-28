#include <Interpreters/GQL/GQLPlanner.h>

#include <Analyzer/IQueryTreeNode.h>
#include <Core/Block.h>
#include <Core/Settings.h>
#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Interpreters/GQL/GQLPlanBuilder.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/ASTSelectIntersectExceptQuery.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/DistinctStep.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/IntersectOrExceptStep.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Processors/QueryPlan/UnionStep.h>
#include <QueryPipeline/SizeLimits.h>
#include <Common/Exception.h>

#include <optional>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace Setting
{
extern const SettingsOverflowMode distinct_overflow_mode;
extern const SettingsMaxThreads max_threads;
extern const SettingsUInt64 max_bytes_in_distinct;
extern const SettingsUInt64 max_rows_in_distinct;
}

namespace GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

enum class CombinedPlanMode : UInt8
{
    UnionAll,
    UnionDistinct,
    ExceptAll,
    ExceptDistinct,
    IntersectAll,
    IntersectDistinct,
};

Names getHeaderColumnNames(const Block & header)
{
    Names names;
    names.reserve(header.columns());
    for (size_t i = 0; i < header.columns(); ++i)
        names.push_back(header.getByPosition(i).name);

    return names;
}

CombinedPlanMode getCombinedPlanMode(const GAST::GQLCombinedQuery & query)
{
    if (query.queries.size() < 2)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL combined query must contain at least two subqueries");
    if (query.operators.size() + 1 != query.queries.size())
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query has inconsistent operator count");

    std::optional<CombinedPlanMode> mode;
    for (const auto operation : query.operators)
    {
        CombinedPlanMode current_mode;
        if (operation == GAST::CombinedQueryOperator::UnionAll)
            current_mode = CombinedPlanMode::UnionAll;
        else if (operation == GAST::CombinedQueryOperator::UnionDistinct)
            current_mode = CombinedPlanMode::UnionDistinct;
        else if (operation == GAST::CombinedQueryOperator::ExceptAll)
            current_mode = CombinedPlanMode::ExceptAll;
        else if (operation == GAST::CombinedQueryOperator::ExceptDistinct)
            current_mode = CombinedPlanMode::ExceptDistinct;
        else if (operation == GAST::CombinedQueryOperator::IntersectAll)
            current_mode = CombinedPlanMode::IntersectAll;
        else if (operation == GAST::CombinedQueryOperator::IntersectDistinct)
            current_mode = CombinedPlanMode::IntersectDistinct;
        else
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL OTHERWISE combined queries are not supported");

        if (mode && *mode != current_mode)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Mixing GQL combined query operators is not supported");

        mode = current_mode;
    }

    if (!mode)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query has no operator");

    return *mode;
}

ASTSelectIntersectExceptQuery::Operator getIntersectOrExceptOperator(CombinedPlanMode mode)
{
    switch (mode)
    {
        case CombinedPlanMode::ExceptAll:
            return ASTSelectIntersectExceptQuery::Operator::EXCEPT_ALL;
        case CombinedPlanMode::ExceptDistinct:
            return ASTSelectIntersectExceptQuery::Operator::EXCEPT_DISTINCT;
        case CombinedPlanMode::IntersectAll:
            return ASTSelectIntersectExceptQuery::Operator::INTERSECT_ALL;
        case CombinedPlanMode::IntersectDistinct:
            return ASTSelectIntersectExceptQuery::Operator::INTERSECT_DISTINCT;
        case CombinedPlanMode::UnionAll:
        case CombinedPlanMode::UnionDistinct:
            break;
    }

    throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query mode is not INTERSECT or EXCEPT");
}

bool needsDistinctStep(CombinedPlanMode mode)
{
    return mode == CombinedPlanMode::UnionDistinct
        || mode == CombinedPlanMode::ExceptDistinct
        || mode == CombinedPlanMode::IntersectDistinct;
}

void buildGQLQueryPlanImpl(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanScope * initial_scope,
    PlanScope * output_scope);

QueryPlanPtr buildChildPlan(
    const ASTPtr & query,
    ContextPtr context,
    const PlanScope * initial_scope)
{
    if (!query)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined child query is null");

    auto plan = std::make_unique<QueryPlan>();
    buildGQLQueryPlanImpl(*plan, *query, context, initial_scope, nullptr);
    return plan;
}

void addConversionIfNeeded(QueryPlan & plan, const SharedHeader & result_header, ContextPtr context)
{
    if (blocksHaveEqualStructure(*plan.getCurrentHeader(), *result_header))
        return;

    auto actions = ActionsDAG::makeConvertingActions(
        plan.getCurrentHeader()->getColumnsWithTypeAndName(),
        result_header->getColumnsWithTypeAndName(),
        ActionsDAG::MatchColumnsMode::Position,
        context);
    auto converting_step = std::make_unique<ExpressionStep>(plan.getCurrentHeader(), std::move(actions));
    converting_step->setStepDescription("Conversion before GQL UNION");
    plan.addStep(std::move(converting_step));
}

void addDistinctStep(QueryPlan & query_plan, const Names & columns, ContextPtr context)
{
    const auto & settings = context->getSettingsRef();
    SizeLimits limits(
        settings[Setting::max_rows_in_distinct],
        settings[Setting::max_bytes_in_distinct],
        settings[Setting::distinct_overflow_mode]);

    query_plan.addStep(std::make_unique<DistinctStep>(query_plan.getCurrentHeader(), limits, 0, columns, false));
}

void buildSingleQueryPlan(
    QueryPlan & query_plan,
    const GAST::GQLSingleQuery & query,
    ContextPtr context,
    const PlanScope * initial_scope,
    PlanScope * output_scope)
{
    if (initial_scope)
    {
        GQLPlanBuilder builder(context, *initial_scope);
        builder.buildSingleQuery(query_plan, query);
        if (output_scope)
            *output_scope = builder.getScope();
        return;
    }

    GQLPlanBuilder builder(context);
    builder.buildSingleQuery(query_plan, query);
    if (output_scope)
        *output_scope = builder.getScope();
}

void buildCombinedQueryPlan(
    QueryPlan & query_plan,
    const GAST::GQLCombinedQuery & query,
    ContextPtr context,
    const PlanScope * initial_scope,
    PlanScope * output_scope)
{
    const auto mode = getCombinedPlanMode(query);
    const size_t num_plans = query.queries.size();
    std::vector<QueryPlanPtr> plans;
    plans.reserve(num_plans);

    for (const auto & child : query.queries)
        plans.push_back(buildChildPlan(child, context, initial_scope));

    const auto result_header = plans.front()->getCurrentHeader();
    const Names result_columns = getHeaderColumnNames(*result_header);

    SharedHeaders headers;
    headers.reserve(plans.size());
    for (auto & plan : plans)
    {
        addConversionIfNeeded(*plan, result_header, context);
        headers.push_back(plan->getCurrentHeader());
    }

    const auto & settings = context->getSettingsRef();
    if (mode == CombinedPlanMode::UnionAll || mode == CombinedPlanMode::UnionDistinct)
    {
        query_plan.unitePlans(std::make_unique<UnionStep>(std::move(headers), settings[Setting::max_threads]), std::move(plans));
    }
    else
    {
        query_plan.unitePlans(
            std::make_unique<IntersectOrExceptStep>(std::move(headers), getIntersectOrExceptOperator(mode), settings[Setting::max_threads]),
            std::move(plans));
    }

    if (needsDistinctStep(mode))
        addDistinctStep(query_plan, result_columns, context);

    if (output_scope)
        output_scope->replaceWithHeader(*query_plan.getCurrentHeader(), BindingKind::Projection);
}

void buildGQLQueryPlanImpl(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    const PlanScope * initial_scope,
    PlanScope * output_scope)
{
    if (const auto * single_query = query.as<GAST::GQLSingleQuery>())
    {
        buildSingleQueryPlan(query_plan, *single_query, context, initial_scope, output_scope);
        return;
    }

    if (const auto * combined_query = query.as<GAST::GQLCombinedQuery>())
    {
        buildCombinedQueryPlan(query_plan, *combined_query, context, initial_scope, output_scope);
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL query root: {}", query.getID(' '));
}

}

void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context)
{
    buildGQLQueryPlanImpl(query_plan, query, std::move(context), nullptr, nullptr);
}

void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const IAST & query,
    ContextPtr context,
    PlanScope & scope)
{
    const auto initial_scope = scope;
    buildGQLQueryPlanImpl(query_plan, query, std::move(context), &initial_scope, &scope);
}

// New QueryTree-based interface
void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const QueryTreeNodePtr & query_tree,
    ContextPtr context)
{
    if (!query_tree)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL QueryTree is null");

    // TODO: Implement direct QueryTree -> QueryPlan conversion
    // For now, convert back to AST and use the existing path
    // This is a temporary bridge until we implement the full QueryTree planner

    auto ast = query_tree->toAST();
    buildGQLQueryPlan(query_plan, *ast, std::move(context));
}

void buildGQLQueryPlan(
    QueryPlan & query_plan,
    const QueryTreeNodePtr & query_tree,
    ContextPtr context,
    PlanScope & scope)
{
    if (!query_tree)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL QueryTree is null");

    // TODO: Implement direct QueryTree -> QueryPlan conversion with scope
    // For now, convert back to AST and use the existing path

    auto ast = query_tree->toAST();
    buildGQLQueryPlan(query_plan, *ast, std::move(context), scope);
}

}

}
