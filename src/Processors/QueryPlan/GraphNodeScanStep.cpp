#include <Processors/QueryPlan/GraphNodeScanStep.h>

#include <Columns/ColumnsNumber.h>
#include <DataTypes/DataTypesNumber.h>
#include <Processors/Sources/GraphNodeScanSource.h>
#include <QueryPipeline/Pipe.h>
#include <QueryPipeline/QueryPipelineBuilder.h>

namespace DB
{

GraphNodeScanStep::GraphNodeScanStep(String node_variable_)
    : ISourceStep(makeHeader(node_variable_))
    , node_variable(std::move(node_variable_))
{
    setStepDescription("GQL MATCH node scan");
}

SharedHeader GraphNodeScanStep::makeHeader(const String & node_variable)
{
    return std::make_shared<const Block>(
        Block{{ColumnUInt64::create(), std::make_shared<DataTypeUInt64>(), node_variable}});
}

QueryPlanStepPtr GraphNodeScanStep::clone() const
{
    return std::make_unique<GraphNodeScanStep>(node_variable);
}

void GraphNodeScanStep::initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &)
{
    pipeline.init(Pipe(std::make_shared<GraphNodeScanSource>(getOutputHeader())));
}

}
