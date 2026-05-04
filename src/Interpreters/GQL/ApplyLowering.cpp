#include <Interpreters/GQL/ApplyLowering.h>

#include <Interpreters/GQL/SourceLowering.h>
#include <Common/Exception.h>

namespace DB::ErrorCodes
{
extern const int LOGICAL_ERROR;
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{

void lowerCorrelatedSourceClause(
    QueryPlan &,
    const ASTPtr & clause,
    ContextPtr,
    const PlanEnvironment &,
    PlanScope &,
    std::string_view context_name)
{
    if (!isSourceIntroducingClause(clause))
        throw Exception(ErrorCodes::LOGICAL_ERROR, "{} expected a source-introducing clause", context_name);

    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "{} source clause is not supported", context_name);
}

}
