#include <Processors/QueryPlan/Graph/MatchStep.h>

#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <Parsers/IAST.h>
#include <Processors/Sources/Graph/MatchSource.h>
#include <QueryPipeline/Pipe.h>
#include <QueryPipeline/QueryPipelineBuilder.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace DB::Graph
{
namespace
{

void addVariableColumn(Block & header, std::vector<String> & names, const String & variable)
{
    if (variable.empty())
        return;

    if (std::find(names.begin(), names.end(), variable) != names.end())
        return;

    names.push_back(variable);
    header.insert({ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), variable});
}

ASTPtr cloneOrNull(const ASTPtr & ast)
{
    return ast ? ast->clone() : nullptr;
}

ASTs cloneASTs(const ASTs & asts)
{
    ASTs result;
    result.reserve(asts.size());
    for (const auto & ast : asts)
    {
        if (ast)
            result.push_back(ast->clone());
    }

    return result;
}

void addAvailableVariable(std::vector<String> & names, const String & variable)
{
    if (variable.empty())
        return;

    if (std::find(names.begin(), names.end(), variable) != names.end())
        return;

    names.push_back(variable);
}

template <typename MatchLike>
std::vector<String> collectAvailableVariables(const MatchLike & match_spec)
{
    std::vector<String> result;
    for (const auto & path : match_spec.paths)
    {
        for (size_t i = 0; i < path.nodes.size(); ++i)
        {
            addAvailableVariable(result, path.nodes[i].variable);
            if (i < path.edges.size())
                addAvailableVariable(result, path.edges[i].variable);
        }
    }

    return result;
}

MatchNodeSpec cloneNodeSpec(const MatchNodeSpec & node)
{
    return MatchNodeSpec{
        .variable = node.variable,
        .label_expression = cloneOrNull(node.label_expression),
        .properties = cloneOrNull(node.properties),
        .predicate = cloneOrNull(node.predicate),
    };
}

MatchEdgeSpec cloneEdgeSpec(const MatchEdgeSpec & edge)
{
    return MatchEdgeSpec{
        .variable = edge.variable,
        .direction = edge.direction,
        .label_expression = cloneOrNull(edge.label_expression),
        .properties = cloneOrNull(edge.properties),
        .predicate = cloneOrNull(edge.predicate),
        .quantifier = cloneOrNull(edge.quantifier),
    };
}

MatchClauseSpec cloneMatchClauseSpec(const MatchClauseSpec & match_spec)
{
    MatchClauseSpec result;
    result.optional = match_spec.optional;
    result.has_match_mode = match_spec.has_match_mode;
    result.match_mode = match_spec.match_mode;
    result.has_keep_clause = match_spec.has_keep_clause;
    result.has_optional_operand_block = match_spec.has_optional_operand_block;
    result.has_yield_items = match_spec.has_yield_items;
    result.keep_clause = cloneOrNull(match_spec.keep_clause);
    result.where_clause = cloneOrNull(match_spec.where_clause);
    result.yield_items = cloneASTs(match_spec.yield_items);
    result.yield_variables = match_spec.yield_variables;

    result.paths.reserve(match_spec.paths.size());
    for (const auto & path : match_spec.paths)
    {
        MatchPathSpec result_path;
        result_path.variable = path.variable;
        result_path.prefix = cloneOrNull(path.prefix);
        result_path.nodes.reserve(path.nodes.size());
        for (const auto & node : path.nodes)
            result_path.nodes.push_back(cloneNodeSpec(node));

        result_path.edges.reserve(path.edges.size());
        for (const auto & edge : path.edges)
            result_path.edges.push_back(cloneEdgeSpec(edge));

        result.paths.push_back(std::move(result_path));
    }

    return result;
}

MatchSpec cloneMatchSpec(const MatchSpec & match_spec)
{
    MatchSpec result;
    static_cast<MatchClauseSpec &>(result) = cloneMatchClauseSpec(match_spec);
    result.clauses.reserve(match_spec.clauses.size());
    for (const auto & clause : match_spec.clauses)
        result.clauses.push_back(cloneMatchClauseSpec(clause));

    return result;
}

void addHeaderColumnsForClause(Block & header, std::vector<String> & names, const MatchClauseSpec & clause)
{
    if (!clause.yield_variables.empty())
    {
        const auto available_names = collectAvailableVariables(clause);
        for (const auto & variable : clause.yield_variables)
        {
            if (std::find(available_names.begin(), available_names.end(), variable) != available_names.end())
                addVariableColumn(header, names, variable);
        }

        return;
    }

    for (const auto & path : clause.paths)
    {
        for (size_t i = 0; i < path.nodes.size(); ++i)
        {
            addVariableColumn(header, names, path.nodes[i].variable);
            if (i < path.edges.size())
                addVariableColumn(header, names, path.edges[i].variable);
        }
    }
}

}

MatchStep::MatchStep(MatchSpec match_spec_)
    : ISourceStep(makeHeader(match_spec_))
    , match_spec(std::move(match_spec_))
{
    setStepDescription("GQL MATCH");
}

SharedHeader MatchStep::makeHeader(const MatchSpec & match_spec)
{
    Block header;
    std::vector<String> names;

    if (!match_spec.clauses.empty())
    {
        for (const auto & clause : match_spec.clauses)
            addHeaderColumnsForClause(header, names, clause);

        return std::make_shared<const Block>(std::move(header));
    }

    if (!match_spec.yield_variables.empty())
    {
        const auto available_names = collectAvailableVariables(match_spec);
        for (const auto & variable : match_spec.yield_variables)
        {
            if (std::find(available_names.begin(), available_names.end(), variable) != available_names.end())
                addVariableColumn(header, names, variable);
        }

        return std::make_shared<const Block>(std::move(header));
    }

    for (const auto & path : match_spec.paths)
    {
        for (size_t i = 0; i < path.nodes.size(); ++i)
        {
            addVariableColumn(header, names, path.nodes[i].variable);
            if (i < path.edges.size())
                addVariableColumn(header, names, path.edges[i].variable);
        }
    }

    return std::make_shared<const Block>(std::move(header));
}

QueryPlanStepPtr MatchStep::clone() const
{
    return std::make_unique<MatchStep>(cloneMatchSpec(match_spec));
}

void MatchStep::initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &)
{
    pipeline.init(Pipe(std::make_shared<MatchSource>(getOutputHeader(), match_spec)));
}

}
