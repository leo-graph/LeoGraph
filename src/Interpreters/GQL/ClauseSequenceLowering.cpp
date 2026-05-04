#include <Interpreters/GQL/ClauseSequenceLowering.h>

#include <Interpreters/GQL/CallLowering.h>
#include <Interpreters/GQL/CatalogLowering.h>
#include <Interpreters/GQL/MutationLowering.h>
#include <Interpreters/GQL/PipelineLowering.h>
#include <Interpreters/GQL/PlanEnvironment.h>
#include <Interpreters/GQL/PlanScope.h>
#include <Interpreters/GQL/SourceLowering.h>
#include <Parsers/IAST.h>
#include <Parsers/graph/GraphAST.h>
#include <Common/Exception.h>

#include <utility>

namespace DB::ErrorCodes
{
extern const int NOT_IMPLEMENTED;
}

namespace DB::GQL
{
namespace
{

class ClauseSequenceLowerer final
{
public:
    ClauseSequenceLowerer(QueryPlan & plan_, ContextPtr context_, const PlanEnvironment & environment_, PlanScope & scope_)
        : plan(plan_)
        , context(std::move(context_))
        , environment(environment_)
        , scope(scope_)
    {
    }

    void lower(const ASTs & clauses)
    {
        if (clauses.empty())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL query must contain at least one clause");

        for (const auto & clause : clauses)
            lowerClause(clause);

        if (source_buffer.hasPending())
            source_buffer.flush(plan, context, environment, scope);
    }

private:
    void lowerClause(const ASTPtr & clause)
    {
        if (const auto * use = clause->as<OPENGQL::AST::GQLUseClause>())
        {
            lowerUseClauseBeforeSource(*use);
            return;
        }

        if (source_buffer.tryAppend(clause))
        {
            if (source_ready)
                throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL source clauses after pipeline clauses are not supported");

            return;
        }

        flushPendingSource();

        if (!source_ready)
        {
            if (tryLowerClauseBeforeSource(clause))
                return;

            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL clause before a source clause is not supported: {}", clause->getID(' '));
        }

        lowerClauseAfterSource(clause);
    }

    void lowerUseClauseBeforeSource(const OPENGQL::AST::GQLUseClause & use)
    {
        if (source_ready || source_buffer.hasPending())
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL USE after a source clause is not supported");

        lowerUseClause(use, scope);
    }

    void flushPendingSource()
    {
        if (!source_buffer.hasPending())
            return;

        source_buffer.flush(plan, context, environment, scope);
        source_ready = true;
    }

    bool tryLowerClauseBeforeSource(const ASTPtr & clause)
    {
        if (tryLowerCatalogClause(plan, clause, context, environment, scope))
        {
            source_ready = true;
            return true;
        }

        if (tryLowerDataModifyingClause(plan, clause, context, environment, scope))
        {
            source_ready = true;
            return true;
        }

        if (tryLowerStandaloneCallClause(plan, clause, context, environment, scope))
        {
            source_ready = true;
            return true;
        }

        if (tryLowerStandaloneSourceClause(plan, clause, context, environment, scope))
        {
            source_ready = true;
            return true;
        }

        return false;
    }

    void lowerClauseAfterSource(const ASTPtr & clause)
    {
        if (isSourceIntroducingClause(clause))
            throw Exception(ErrorCodes::NOT_IMPLEMENTED, "GQL source clauses after pipeline clauses are not supported");

        lowerPipelinePositionClause(plan, clause, context, environment, scope);
    }

    QueryPlan & plan;
    ContextPtr context;
    const PlanEnvironment & environment;
    PlanScope & scope;
    SourceClauseBuffer source_buffer;
    bool source_ready = false;
};

}

void lowerClauseSequence(
    QueryPlan & plan,
    const ASTs & clauses,
    ContextPtr context,
    const PlanEnvironment & environment,
    PlanScope & scope)
{
    ClauseSequenceLowerer(plan, std::move(context), environment, scope).lower(clauses);
}

}
