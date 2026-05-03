#include <Processors/QueryPlan/Graph/MatchStep.h>

#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
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
    return std::make_unique<MatchStep>(match_spec);
}

void MatchStep::initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &)
{
    pipeline.init(Pipe(std::make_shared<MatchSource>(getOutputHeader(), match_spec)));
}

}
