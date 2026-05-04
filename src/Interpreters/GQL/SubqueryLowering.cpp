#include <Interpreters/GQL/SubqueryLowering.h>

#include <Interpreters/GQL/PlanBuilder.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
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

ASTPtr makeValueBindingExpression(const GAST::GQLBindingVariableDefinition & definition, std::string_view context_name)
{
    const auto * initializer = definition.initializer ? definition.initializer->as<GAST::GQLBindingInitializer>() : nullptr;
    if (!initializer)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} VALUE binding '{}' must have an initializer", context_name, definition.name);
    if (initializer->kind != GAST::GQLBindingInitializer::Kind::Value)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding '{}' must have a VALUE initializer", context_name, definition.name);
    if (!initializer->value)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} VALUE binding '{}' must have a value expression", context_name, definition.name);

    auto expression = initializer->value->clone();
    if (definition.type)
        expression = GAST::GQLExpr::castExpr(std::move(expression), definition.type->clone());

    return expression;
}

}

const OPENGQL::AST::GQLSingleQuery & getSingleQuerySubquery(
    const OPENGQL::AST::GQLSubquery & subquery,
    std::string_view context_name)
{
    if (subquery.at_schema)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} AT schema is not supported", context_name);
    if (!subquery.next_statements.empty())
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} NEXT statements are not supported", context_name);

    const auto * single_query = subquery.query ? subquery.query->as<GAST::GQLSingleQuery>() : nullptr;
    if (!single_query)
        throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} must contain a single query", context_name);

    return *single_query;
}

void lowerSubqueryBindings(
    const OPENGQL::AST::GQLBindingVariableDefinitionBlock & block,
    PlanScope & scope,
    std::string_view context_name)
{
    std::unordered_set<String> names;
    for (const auto & definition_ast : block.definitions)
    {
        const auto * definition = definition_ast ? definition_ast->as<GAST::GQLBindingVariableDefinition>() : nullptr;
        if (!definition)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding must be GQLBindingVariableDefinition", context_name);
        if (definition->name.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding name must be non-empty", context_name);
        if (!names.insert(definition->name).second)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Duplicate {} binding '{}'", context_name, definition->name);
        if (scope.hasBinding(definition->name))
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} binding '{}' conflicts with an outer binding", context_name, definition->name);

        if (definition->kind != GAST::GQLBindingVariableDefinition::Kind::Value)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} only supports VALUE bindings", context_name);

        scope.addOrReplaceBinding(
            definition->name,
            nullptr,
            BindingKind::Projection,
            makeValueBindingExpression(*definition, context_name));
    }
}

void lowerSubquerySource(
    QueryPlan & plan,
    const OPENGQL::AST::GQLSubquery & subquery,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope,
    PlanScope child_scope,
    std::string_view context_name)
{
    const auto & single_query = getSingleQuerySubquery(subquery, context_name);

    if (subquery.bindings)
    {
        const auto * binding_block = subquery.bindings->as<GAST::GQLBindingVariableDefinitionBlock>();
        if (!binding_block)
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} bindings must be GQLBindingVariableDefinitionBlock", context_name);

        lowerSubqueryBindings(*binding_block, child_scope, context_name);
    }

    PlanBuilder nested_builder(context, std::move(child_scope), environment);
    nested_builder.buildSingleQuery(plan, single_query);
    scope = nested_builder.getScope();
}

}
