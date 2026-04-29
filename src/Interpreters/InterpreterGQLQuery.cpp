#include <Interpreters/InterpreterGQLQuery.h>

#include <Interpreters/Context.h>
#include <Interpreters/InterpreterFactory.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/GraphNodeScanStep.h>
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

const GAST::GQLExpr * getIdentifierExpression(const ASTPtr & ast)
{
    if (!ast)
        return nullptr;

    if (const auto * item = ast->as<GAST::GQLAliasedItem>())
        return getIdentifierExpression(item->expression);

    const auto * expression = ast->as<GAST::GQLExpr>();
    if (!expression || expression->kind != GAST::GQLExpr::Kind::Identifier)
        return nullptr;

    return expression;
}

String extractMinimalNodeVariable(const GAST::GQLMatchClause & match)
{
    if (match.optional || match.match_mode != GAST::GraphMatchMode::None || match.keep_clause || match.where || match.optional_operand_block
        || !match.yield_items.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only basic GQL MATCH clauses are supported");

    if (match.path_patterns.size() != 1)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only a single GQL MATCH path pattern is supported");

    const auto * path = match.path_patterns.front()->as<GAST::GQLPathPattern>();
    if (!path || path->variable || path->prefix)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only an unqualified GQL MATCH path pattern is supported");

    const auto * term = path->expression ? path->expression->as<GAST::GQLPathTerm>() : nullptr;
    if (!term || term->factors.size() != 1)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only a single node GQL MATCH pattern is supported");

    const auto * node = term->factors.front()->as<GAST::GQLNodePattern>();
    if (!node || node->label_expression || node->properties || node->where)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only an unlabeled GQL MATCH node pattern is supported");

    const auto * variable = getIdentifierExpression(node->variable);
    if (!variable || variable->text.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only named GQL MATCH node patterns are supported");

    return variable->text;
}

void validateMinimalReturn(const GAST::GQLReturnClause & return_clause, const String & node_variable)
{
    if (return_clause.distinct || return_clause.return_all || return_clause.group_by)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only basic GQL RETURN clauses are supported");

    if (return_clause.items.size() != 1)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only a single GQL RETURN item is supported");

    const auto * item = return_clause.items.front()->as<GAST::GQLAliasedItem>();
    if (!item || !item->alias.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only unaliased GQL RETURN items are supported");

    const auto * expression = getIdentifierExpression(item->expression);
    if (!expression || expression->text != node_variable)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only returning the matched GQL node variable is supported");
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
    executeMatch(query_plan);
    executeWhere(query_plan);
    executeReturn(query_plan);
    executePage(query_plan);
    query_plan.addInterpreterContext(getContext());
}

void InterpreterGQLQuery::executeMatch(QueryPlan & query_plan)
{
    const auto & query = getSingleQuery(query_ptr);
    if (query.clauses.size() != 2)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL MATCH followed by RETURN is supported");

    const auto * match = query.clauses[0]->as<GAST::GQLMatchClause>();
    if (!match)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL queries starting with MATCH are supported");

    const String node_variable = extractMinimalNodeVariable(*match);
    query_plan.addStep(std::make_unique<GraphNodeScanStep>(node_variable));
}

void InterpreterGQLQuery::executeWhere(QueryPlan &)
{
}

void InterpreterGQLQuery::executeReturn(QueryPlan &)
{
    const auto & query = getSingleQuery(query_ptr);
    const auto * match = query.clauses[0]->as<GAST::GQLMatchClause>();
    chassert(match);

    const String node_variable = extractMinimalNodeVariable(*match);
    const auto * return_clause = query.clauses[1]->as<GAST::GQLReturnClause>();
    if (!return_clause)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL MATCH followed by RETURN is supported");

    validateMinimalReturn(*return_clause, node_variable);
}

void InterpreterGQLQuery::executePage(QueryPlan &)
{
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
