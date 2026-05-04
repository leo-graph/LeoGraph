#include <Interpreters/GQL/CallLowering.h>

#include <Interpreters/GQL/ClauseLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Interpreters/GQL/SourceLowering.h>
#include <Interpreters/GQL/SubqueryLowering.h>
#include <Parsers/graph/GraphAST.h>
#include <Processors/QueryPlan/QueryPlan.h>
#include <Common/Exception.h>

#include <unordered_set>
#include <utility>

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

namespace GAST = DB::OPENGQL::AST;

Names extractInlineCallVariableScope(const GAST::GQLCallInlineClause & call)
{
    Names imports;
    if (!call.variable_scope)
        return imports;

    const auto * variable_scope = call.variable_scope->as<GAST::GQLCallVariableScopeClause>();
    if (!variable_scope)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope must be GQLCallVariableScopeClause");

    std::unordered_set<String> seen;
    imports.reserve(variable_scope->variables.size());
    for (const auto & variable_ast : variable_scope->variables)
    {
        const auto * variable = variable_ast ? variable_ast->as<GAST::GQLExpr>() : nullptr;
        if (!variable || variable->kind != GAST::GQLExpr::Kind::Identifier || variable->text.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope must contain binding-variable identifiers");
        if (!seen.insert(variable->text).second)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Duplicate inline CALL variable scope import '{}'", variable->text);

        imports.push_back(variable->text);
    }

    return imports;
}

PlanScope makeInlineCallSourceScope(const PlanScope & scope, const Names & imports)
{
    auto child_scope = scope.makeChildGraphScope();
    for (const auto & name : imports)
    {
        const auto * binding = scope.tryGetBinding(name);
        if (!binding)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' is not available", name);
        if (!binding->expression)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' requires correlation semantics", name);

        child_scope.addOrReplaceBinding(name, binding->type, binding->kind, binding->expression->clone());
    }

    return child_scope;
}

PlanScope makeInlineCallPipelineScope(const PlanScope & scope, const Names & imports)
{
    auto child_scope = scope.makeChildGraphScope();
    for (const auto & name : imports)
    {
        const auto * binding = scope.tryGetBinding(name);
        if (!binding)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL variable scope binding '{}' is not available", name);

        child_scope.addOrReplaceBinding(
            name,
            binding->type,
            binding->kind,
            binding->expression ? binding->expression->clone() : ASTPtr{});
    }

    return child_scope;
}

const GAST::GQLSubquery & getInlineCallSubquery(const GAST::GQLCallInlineClause & call)
{
    if (call.optional)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "OPTIONAL inline CALL is not supported");

    const auto * subquery = call.subquery ? call.subquery->as<GAST::GQLSubquery>() : nullptr;
    if (!subquery)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "inline CALL must contain a subquery");

    return *subquery;
}

void lowerInlineCallPipelineClauseImpl(
    QueryPlan & plan,
    const GAST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope);

void lowerPipelineOnlySubquery(
    QueryPlan & plan,
    const GAST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    std::string_view context_name)
{
    const auto & single_query = getSingleQuerySubquery(subquery, context_name);
    if (single_query.clauses.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain at least one clause", context_name);

    if (subquery.bindings)
    {
        const auto * binding_block = subquery.bindings->as<GAST::GQLBindingVariableDefinitionBlock>();
        if (!binding_block)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings must be GQLBindingVariableDefinitionBlock", context_name);

        lowerSubqueryBindings(*binding_block, scope, context_name);
    }

    for (const auto & clause : single_query.clauses)
    {
        if (const auto * use = clause->as<GAST::GQLUseClause>())
        {
            lowerUseClause(*use, scope);
            continue;
        }

        if (clause->as<GAST::GQLMatchClause>())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} source clause is not supported", context_name);

        if (const auto * select = clause->as<GAST::GQLSelectClause>(); select && select->source)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} source clause is not supported", context_name);

        if (const auto * inline_call = clause->as<GAST::GQLCallInlineClause>())
        {
            lowerInlineCallPipelineClauseImpl(plan, *inline_call, context, environment, scope);
            continue;
        }

        lowerPipelineClause(plan, clause, context, scope);
    }
}

void lowerInlineCallPipelineClauseImpl(
    QueryPlan & plan,
    const GAST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    const auto & subquery = getInlineCallSubquery(call);
    auto child_scope = makeInlineCallPipelineScope(scope, extractInlineCallVariableScope(call));
    lowerPipelineOnlySubquery(plan, subquery, context, environment, child_scope, "pipeline inline CALL subquery");
    scope = std::move(child_scope);
}

}

void lowerInlineCallSource(
    QueryPlan & plan,
    const OPENGQL::AST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    const auto & subquery = getInlineCallSubquery(call);
    lowerSubquerySource(
        plan,
        subquery,
        context,
        environment,
        scope,
        makeInlineCallSourceScope(scope, extractInlineCallVariableScope(call)),
        "inline CALL subquery");
}

void lowerInlineCallPipelineClause(
    QueryPlan & plan,
    const OPENGQL::AST::GQLCallInlineClause & call,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    lowerInlineCallPipelineClauseImpl(plan, call, context, environment, scope);
}

}
