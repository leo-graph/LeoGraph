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

MatchSpec cloneMatchSpec(const MatchSpec & match_spec)
{
    MatchSpec result;
    result.optional = match_spec.optional;
    result.has_match_mode = match_spec.has_match_mode;
    result.has_keep_clause = match_spec.has_keep_clause;
    result.has_optional_operand_block = match_spec.has_optional_operand_block;
    result.has_yield_items = match_spec.has_yield_items;

    result.paths.reserve(match_spec.paths.size());
    for (const auto & path : match_spec.paths)
    {
        MatchPathSpec result_path;
        result_path.variable = path.variable;
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
