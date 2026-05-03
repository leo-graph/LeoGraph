#include <Interpreters/InterpreterGQLQuery.h>

#include <charconv>

#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Interpreters/GQL/ExpressionLowering.h>
#include <Interpreters/GQL/MatchPlanToSpec.h>
#include <Interpreters/GQL/PatternLowering.h>
#include <Interpreters/InterpreterFactory.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/ExpressionStep.h>
#include <Processors/QueryPlan/FilterStep.h>
#include <Processors/QueryPlan/Graph/MatchStep.h>
#include <Processors/QueryPlan/LimitStep.h>
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

void validateSingleNodeExecutableMatch(const Graph::MatchSpec & match)
{
    if (match.optional || match.has_optional_operand_block)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL MATCH is not supported");
    if (match.has_match_mode)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH mode prefix is not supported");
    if (match.has_keep_clause)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH KEEP is not supported");
    if (match.has_yield_items)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL MATCH YIELD is not supported");
    if (match.paths.size() != 1)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only a single GQL MATCH path pattern is supported");

    const auto & path = match.paths.front();
    if (path.nodes.size() != 1 || !path.edges.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only single-node GQL MATCH patterns are supported");

    const auto & node = path.nodes.front();
    if (node.has_label_expression || node.has_properties || node.has_predicate)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Node label, properties, and per-node WHERE are not yet supported");
    if (node.variable.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Anonymous GQL MATCH node patterns are not supported");
}

void lowerWherePredicate(QueryPlan & plan, const GAST::GQLExpr & predicate, ContextPtr context)
{
    auto current_header = plan.getCurrentHeader();
    ActionsDAG dag(current_header->getColumnsWithTypeAndName());

    const auto & filter_node = GQL::lowerExpression(predicate, dag, context);
    dag.getOutputs().push_back(&filter_node);

    plan.addStep(std::make_unique<FilterStep>(current_header, std::move(dag), filter_node.result_name, true));
}

void lowerWhereClause(QueryPlan & plan, const GAST::GQLWhereClause & where, ContextPtr context, std::string_view context_name)
{
    if (where.type != GAST::GQLWhereClause::Type::Where)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "HAVING inside {} is not supported", context_name);

    const auto * predicate = where.expression ? where.expression->as<GAST::GQLExpr>() : nullptr;
    if (!predicate)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} predicate must be a GQL expression", context_name);

    lowerWherePredicate(plan, *predicate, context);
}

void lowerMatch(QueryPlan & plan, const GAST::GQLMatchClause & match, ContextPtr context)
{
    const auto match_plan = GQL::lowerMatchClause(match);
    auto match_spec = GQL::makeMatchSpec(match_plan);
    validateSingleNodeExecutableMatch(match_spec);
    plan.addStep(std::make_unique<Graph::MatchStep>(std::move(match_spec)));

    if (match_plan.where)
        lowerWhereClause(plan, *match_plan.where, context, "GQL MATCH");
}

void lowerReturn(QueryPlan & plan, const GAST::GQLReturnClause & ret, ContextPtr context)
{
    if (ret.distinct)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "RETURN DISTINCT is not supported");
    if (ret.group_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "RETURN GROUP BY is not supported");

    if (ret.return_all)
        return;

    if (ret.items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "RETURN must project at least one item");

    auto current_header = plan.getCurrentHeader();
    ActionsDAG dag(current_header->getColumnsWithTypeAndName());

    ActionsDAG::NodeRawConstPtrs new_outputs;
    new_outputs.reserve(ret.items.size());

    for (const auto & item_ast : ret.items)
    {
        const auto * item = item_ast->as<GAST::GQLAliasedItem>();
        if (!item)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL RETURN item must be GQLAliasedItem");

        const auto * expr = item->expression ? item->expression->as<GAST::GQLExpr>() : nullptr;
        if (!expr)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL RETURN item must wrap a GQL expression");

        const auto & node = GQL::lowerExpression(*expr, dag, context);
        if (item->alias.empty())
            new_outputs.push_back(&node);
        else
            new_outputs.push_back(&dag.addAlias(node, item->alias));
    }

    dag.getOutputs() = std::move(new_outputs);
    plan.addStep(std::make_unique<ExpressionStep>(current_header, std::move(dag)));
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

void lowerPage(QueryPlan & plan, const GAST::GQLPageClause & page)
{
    if (page.order_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL ORDER BY is not yet supported");
    if (page.offset)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL OFFSET is not yet supported");
    if (!page.limit)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL page clause without LIMIT is not supported");

    const auto * limit_expr = page.limit->as<GAST::GQLExpr>();
    if (!limit_expr)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL LIMIT must be a GQL expression");

    const UInt64 limit = extractUnsignedIntegerLiteral(*limit_expr, "LIMIT");
    plan.addStep(std::make_unique<LimitStep>(plan.getCurrentHeader(), limit, 0));
}

void lowerClause(QueryPlan & plan, const ASTPtr & clause_ast, ContextPtr context, size_t & match_count)
{
    if (!clause_ast)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL clause node is null");

    if (const auto * match = clause_ast->as<GAST::GQLMatchClause>())
    {
        if (match_count > 0)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Multiple MATCH clauses are not supported");
        ++match_count;
        lowerMatch(plan, *match, context);
        return;
    }

    if (const auto * ret = clause_ast->as<GAST::GQLReturnClause>())
    {
        lowerReturn(plan, *ret, context);
        return;
    }

    if (const auto * page = clause_ast->as<GAST::GQLPageClause>())
    {
        lowerPage(plan, *page);
        return;
    }

    if (const auto * where = clause_ast->as<GAST::GQLWhereClause>())
    {
        lowerWhereClause(plan, *where, context, "GQL FILTER");
        return;
    }

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Unsupported GQL clause: {}", clause_ast->getID(' '));
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

    size_t match_count = 0;
    for (const auto & clause : query.clauses)
        lowerClause(query_plan, clause, getContext(), match_count);

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
