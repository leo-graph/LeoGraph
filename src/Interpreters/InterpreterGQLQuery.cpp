#include <Interpreters/InterpreterGQLQuery.h>

#include <charconv>

#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionFactory.h>
#include <Interpreters/ActionsDAG.h>
#include <Interpreters/Context.h>
#include <Interpreters/InterpreterFactory.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/BuildQueryPipelineSettings.h>
#include <Processors/QueryPlan/FilterStep.h>
#include <Processors/QueryPlan/GraphNodeScanStep.h>
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

struct MinimalMatchExecution
{
    String node_variable;
    const GAST::GQLWhereClause * where = nullptr;
    const GAST::GQLPageClause * page = nullptr;
};

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
    if (match.optional || match.match_mode != GAST::GraphMatchMode::None || match.keep_clause || match.optional_operand_block || !match.yield_items.empty())
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

UInt64 extractUInt64Literal(const ASTPtr & ast, const String & context)
{
    const auto * expression = ast ? ast->as<GAST::GQLExpr>() : nullptr;
    if (!expression || expression->kind != GAST::GQLExpr::Kind::Literal || expression->text.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only unsigned integer GQL {} literals are supported", context);

    UInt64 value = 0;
    const char * begin = expression->text.data();
    const char * end = begin + expression->text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only unsigned integer GQL {} literals are supported", context);

    return value;
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

String mapComparisonFunction(const String & op)
{
    if (op == "=")
        return "equals";
    if (op == "<>" || op == "!=")
        return "notEquals";
    if (op == ">")
        return "greater";
    if (op == ">=")
        return "greaterOrEquals";
    if (op == "<")
        return "less";
    if (op == "<=")
        return "lessOrEquals";

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only basic comparison GQL MATCH filters are supported");
}

ActionsDAG buildNodeIdFilterDAG(
    const SharedHeader & input_header, const GAST::GQLWhereClause & where_clause, const String & node_variable, ContextPtr context)
{
    if (where_clause.type != GAST::GQLWhereClause::Type::Where)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL WHERE filters are supported");

    const auto * predicate = where_clause.expression ? where_clause.expression->as<GAST::GQLExpr>() : nullptr;
    if (!predicate || predicate->kind != GAST::GQLExpr::Kind::BinaryOp || predicate->children.size() != 2)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only binary GQL MATCH filters on the node id are supported");

    const auto * left = getIdentifierExpression(predicate->children[0]);
    if (!left || left->text != node_variable)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL MATCH filters on the matched node variable are supported");

    const UInt64 right_value = extractUInt64Literal(predicate->children[1], "MATCH filter");
    const String function_name = mapComparisonFunction(predicate->text);

    ActionsDAG actions(input_header->getColumnsWithTypeAndName());
    const auto & left_node = actions.findInOutputs(node_variable);

    auto uint64_type = std::make_shared<DataTypeUInt64>();
    const auto & right_node = actions.addColumn(
        {uint64_type->createColumnConst(1, Field(right_value)), uint64_type, "__gql_filter_literal"});

    auto function = FunctionFactory::instance().get(function_name, context);
    const auto & filter_node = actions.addFunction(function, {&left_node, &right_node}, "__gql_filter");
    actions.getOutputs().push_back(&filter_node);

    return actions;
}

MinimalMatchExecution analyzeMinimalMatchExecution(const GAST::GQLSingleQuery & query)
{
    if (query.clauses.size() != 2 && query.clauses.size() != 3)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL MATCH followed by RETURN and optional LIMIT is supported");

    const auto * match = query.clauses[0]->as<GAST::GQLMatchClause>();
    if (!match)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL queries starting with MATCH are supported");

    MinimalMatchExecution execution;
    execution.node_variable = extractMinimalNodeVariable(*match);

    if (match->where)
    {
        execution.where = match->where->as<GAST::GQLWhereClause>();
        if (!execution.where)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL MATCH where node must be GQLWhereClause");
    }

    const auto * return_clause = query.clauses[1]->as<GAST::GQLReturnClause>();
    if (!return_clause)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL MATCH followed by RETURN is supported");

    validateMinimalReturn(*return_clause, execution.node_variable);

    if (query.clauses.size() == 3)
    {
        execution.page = query.clauses[2]->as<GAST::GQLPageClause>();
        if (!execution.page)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL LIMIT is supported after RETURN");

        if (execution.page->order_by || execution.page->offset || !execution.page->limit)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Only GQL LIMIT without ORDER BY or OFFSET is supported");
    }

    return execution;
}

void executeMatch(QueryPlan & query_plan, const MinimalMatchExecution & execution)
{
    query_plan.addStep(std::make_unique<GraphNodeScanStep>(execution.node_variable));
}

void executeWhere(QueryPlan & query_plan, const MinimalMatchExecution & execution, ContextPtr context)
{
    if (!execution.where)
        return;

    auto filter_actions = buildNodeIdFilterDAG(query_plan.getCurrentHeader(), *execution.where, execution.node_variable, context);
    query_plan.addStep(std::make_unique<FilterStep>(query_plan.getCurrentHeader(), std::move(filter_actions), "__gql_filter", true));
}

void executePage(QueryPlan & query_plan, const MinimalMatchExecution & execution)
{
    if (!execution.page)
        return;

    const UInt64 limit = extractUInt64Literal(execution.page->limit, "LIMIT");
    query_plan.addStep(std::make_unique<LimitStep>(query_plan.getCurrentHeader(), limit, 0));
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
    const auto execution = analyzeMinimalMatchExecution(query);

    executeMatch(query_plan, execution);
    executeWhere(query_plan, execution, getContext());
    executePage(query_plan, execution);
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
