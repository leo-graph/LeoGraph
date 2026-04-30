#include <Processors/QueryPlan/Graph/NodeScanStep.h>

#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <Processors/Sources/Graph/NodeScanSource.h>
#include <QueryPipeline/Pipe.h>
#include <QueryPipeline/QueryPipelineBuilder.h>

namespace DB::Graph
{

NodeScanStep::NodeScanStep(String node_variable_)
    : ISourceStep(makeHeader(node_variable_))
    , node_variable(std::move(node_variable_))
{
    setStepDescription("GQL MATCH node scan");
}

SharedHeader NodeScanStep::makeHeader(const String & node_variable)
{
    return std::make_shared<const Block>(
        Block{{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), node_variable}});
}

QueryPlanStepPtr NodeScanStep::clone() const
{
    return std::make_unique<NodeScanStep>(node_variable);
}

void NodeScanStep::initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &)
{
    pipeline.init(Pipe(std::make_shared<NodeScanSource>(getOutputHeader())));
}

}
