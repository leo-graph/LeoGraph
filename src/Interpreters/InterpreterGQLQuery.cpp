#include <Interpreters/InterpreterGQLQuery.h>

#include <vector>

#include <Interpreters/Context.h>
#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/MatchPlanToSpec.h>
#include <Interpreters/GQL/PatternLowering.h>
#include <Interpreters/InterpreterFactory.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/Graph/MatchStep.h>
#include <Processors/QueryPlan/Optimizations/QueryPlanOptimizationSettings.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <QueryPipeline/BlockIO.h>
#include <QueryPipeline/QueryPipelineBuilder.h>

namespace DB
{

namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace
{

namespace GAST = DB::OPENGQL::AST;

const GAST::GQLSingleQuery & getSingleQuery(const ASTPtr & query_ptr)
{
    if (const auto * query = query_ptr->as<GAST::GQLSingleQuery>())
        return *query;

    throw Exception(ErrorCodes::LOGICAL_ERROR, "InterpreterGQLQuery expects GQLSingleQuery");
}

void validateExecutableMatch(const Graph::MatchSpec & match)
{
    if (match.optional || match.has_optional_operand_block)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL MATCH is not supported");
    if (match.paths.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH must contain at least one path pattern");
}

void lowerMatchSequence(QueryPlan & plan, const std::vector<const GAST::GQLMatchClause *> & matches, ContextPtr context)
{
    std::vector<GQL::MatchPlan> match_plans;
    match_plans.reserve(matches.size());
    for (const auto * match : matches)
    {
        if (!match)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL MATCH clause node is null");

        match_plans.push_back(GQL::lowerMatchClause(*match));
    }

    auto match_spec = GQL::makeMatchSpec(match_plans);
    validateExecutableMatch(match_spec);
    plan.addStep(std::make_unique<Graph::MatchStep>(std::move(match_spec)));

    for (const auto & match_plan : match_plans)
    {
        if (match_plan.where)
            GQL::lowerWhereClause(plan, *match_plan.where, context, "GQL MATCH");
    }
}

}

InterpreterGQLQuery::InterpreterGQLQuery(ASTPtr query_ptr_, ContextPtr context_)
    : WithContext(context_)
    , query_ptr(std::move(query_ptr_))
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
    const auto & query = getSingleQuery(query_ptr);

    if (query.clauses.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL query must contain at least one clause");

    if (!query.clauses.front()->as<GAST::GQLMatchClause>())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL queries starting with MATCH are supported");

    std::vector<const GAST::GQLMatchClause *> pending_matches;
    bool match_sequence_closed = false;
    for (const auto & clause : query.clauses)
    {
        if (const auto * match = clause->as<GAST::GQLMatchClause>())
        {
            if (match_sequence_closed)
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "MATCH clauses after non-MATCH clauses are not supported");

            pending_matches.push_back(match);
            continue;
        }

        if (!pending_matches.empty())
        {
            lowerMatchSequence(query_plan, pending_matches, getContext());
            pending_matches.clear();
            match_sequence_closed = true;
        }

        GQL::lowerPipelineClause(query_plan, clause, getContext());
    }

    if (!pending_matches.empty())
        lowerMatchSequence(query_plan, pending_matches, getContext());

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
