#include <Interpreters/InterpreterGQLQuery.h>

#include <Core/Block.h>
#include <Core/Settings.h>
#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Interpreters/GQL/PlanBuilder.h>
#include <Interpreters/InterpreterFactory.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/DistinctStep.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/IntersectOrExceptStep.h>
#include <Processors/QueryPlan/Optimizations/QueryPlanOptimizationSettings.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Processors/QueryPlan/UnionStep.h>
#include <QueryPipeline/SizeLimits.h>
#include <QueryPipeline/BlockIO.h>
#include <QueryPipeline/QueryPipelineBuilder.h>
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

namespace
{

namespace GAST = DB::OPENGQL::AST;

enum class CombinedLoweringMode : UInt8
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

CombinedLoweringMode getCombinedLoweringMode(const GAST::GQLCombinedQuery & query)
{
    if (query.queries.size() < 2)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL combined query must contain at least two subqueries");
    if (query.operators.size() + 1 != query.queries.size())
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query has inconsistent operator count");

    std::optional<CombinedLoweringMode> mode;
    for (const auto operation : query.operators)
    {
        CombinedLoweringMode current_mode;
        if (operation == GAST::CombinedQueryOperator::UnionAll)
            current_mode = CombinedLoweringMode::UnionAll;
        else if (operation == GAST::CombinedQueryOperator::UnionDistinct)
            current_mode = CombinedLoweringMode::UnionDistinct;
        else if (operation == GAST::CombinedQueryOperator::ExceptAll)
            current_mode = CombinedLoweringMode::ExceptAll;
        else if (operation == GAST::CombinedQueryOperator::ExceptDistinct)
            current_mode = CombinedLoweringMode::ExceptDistinct;
        else if (operation == GAST::CombinedQueryOperator::IntersectAll)
            current_mode = CombinedLoweringMode::IntersectAll;
        else if (operation == GAST::CombinedQueryOperator::IntersectDistinct)
            current_mode = CombinedLoweringMode::IntersectDistinct;
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

ASTSelectIntersectExceptQuery::Operator getIntersectOrExceptOperator(CombinedLoweringMode mode)
{
    switch (mode)
    {
        case CombinedLoweringMode::ExceptAll:
            return ASTSelectIntersectExceptQuery::Operator::EXCEPT_ALL;
        case CombinedLoweringMode::ExceptDistinct:
            return ASTSelectIntersectExceptQuery::Operator::EXCEPT_DISTINCT;
        case CombinedLoweringMode::IntersectAll:
            return ASTSelectIntersectExceptQuery::Operator::INTERSECT_ALL;
        case CombinedLoweringMode::IntersectDistinct:
            return ASTSelectIntersectExceptQuery::Operator::INTERSECT_DISTINCT;
        case CombinedLoweringMode::UnionAll:
        case CombinedLoweringMode::UnionDistinct:
            break;
    }

    throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined query mode is not INTERSECT or EXCEPT");
}

bool needsDistinctStep(CombinedLoweringMode mode)
{
    return mode == CombinedLoweringMode::UnionDistinct
        || mode == CombinedLoweringMode::ExceptDistinct
        || mode == CombinedLoweringMode::IntersectDistinct;
}

void buildGQLRootPlan(QueryPlan & query_plan, const IAST & query, ContextPtr context, Graph::MatchSourceFactoryPtr match_source_factory);

QueryPlanPtr buildChildPlan(const ASTPtr & query, ContextPtr context, Graph::MatchSourceFactoryPtr match_source_factory)
{
    if (!query)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL combined child query is null");

    auto plan = std::make_unique<QueryPlan>();
    buildGQLRootPlan(*plan, *query, context, std::move(match_source_factory));
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
    Graph::MatchSourceFactoryPtr match_source_factory)
{
    GQL::PlanBuilder(context, std::move(match_source_factory)).buildSingleQuery(query_plan, query);
}

void buildCombinedQueryPlan(
    QueryPlan & query_plan,
    const GAST::GQLCombinedQuery & query,
    ContextPtr context,
    Graph::MatchSourceFactoryPtr match_source_factory)
{
    const auto mode = getCombinedLoweringMode(query);
    const size_t num_plans = query.queries.size();
    std::vector<QueryPlanPtr> plans;
    plans.reserve(num_plans);

    for (const auto & child : query.queries)
        plans.push_back(buildChildPlan(child, context, match_source_factory));

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
    if (mode == CombinedLoweringMode::UnionAll || mode == CombinedLoweringMode::UnionDistinct)
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
}

void buildGQLRootPlan(QueryPlan & query_plan, const IAST & query, ContextPtr context, Graph::MatchSourceFactoryPtr match_source_factory)
{
    if (const auto * single_query = query.as<GAST::GQLSingleQuery>())
    {
        buildSingleQueryPlan(query_plan, *single_query, context, std::move(match_source_factory));
        return;
    }

    if (const auto * combined_query = query.as<GAST::GQLCombinedQuery>())
    {
        buildCombinedQueryPlan(query_plan, *combined_query, context, std::move(match_source_factory));
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL query root: {}", query.getID(' '));
}

}

InterpreterGQLQuery::InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_, Graph::MatchSourceFactoryPtr match_source_factory_)
    : WithContext(context_)
    , query_ptr(std::move(query_ptr_))
    , match_source_factory(std::move(match_source_factory_))
{
}

BlockIO InterpreterGQLQuery::execute()
{
    BlockIO result;
    QueryPlan query_plan;

    buildQueryPlan(query_plan);

    auto builder = query_plan.buildQueryPipeline(QueryPlanOptimizationSettings(getContext()), BuildQueryPipelineSettings(getContext()));
    result.pipeline = QueryPipelineBuilder::getPipeline(std::move(*builder));

    return result;
}

void InterpreterGQLQuery::buildQueryPlan(QueryPlan & query_plan)
{
    if (!query_ptr)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "InterpreterGQLQuery query is null");

    buildGQLRootPlan(query_plan, *query_ptr, getContext(), match_source_factory);
    query_plan.addInterpreterContext(getContext());
}

void registerInterpreterGQLQuery(InterpreterFactory & factory)
{
    auto create_fn = [](const InterpreterFactory::Arguments & args)
    {
        return std::make_unique<InterpreterGQLQuery>(args.query, args.context);
    };
    factory.registerInterpreter("InterpreterGQLQuery", create_fn);
}

}
