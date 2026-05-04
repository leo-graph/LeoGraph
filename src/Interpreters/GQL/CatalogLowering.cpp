#include <Interpreters/GQL/CatalogLowering.h>

#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Parsers/graph/GraphAST.h>
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

bool isCatalogClause(const ASTPtr & clause)
{
    return clause && clause->as<OPENGQL::AST::GQLCatalogStatement>();
}

void lowerCatalogClause(QueryPlan &, const ASTPtr & clause, ContextPtr, PlanScope &)
{
    if (!clause)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL catalog clause is null");
    if (!isCatalogClause(clause))
        throw Exception(ErrorCodes::LOGICAL_ERROR, "GQL catalog lowering received {}", clause->getID(' '));

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL catalog execution is not supported");
}

}

bool tryLowerCatalogClause(QueryPlan & plan, const ASTPtr & clause, ContextPtr context, const PlanEnvironment &, PlanScope & scope)
{
    if (!isCatalogClause(clause))
        return false;

    lowerCatalogClause(plan, clause, context, scope);
    return true;
}

}

}
