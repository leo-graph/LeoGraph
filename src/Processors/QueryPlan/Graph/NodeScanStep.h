#pragma once

#include <Processors/QueryPlan/ISourceStep.h>

namespace DB::Graph
{

class NodeScanStep final : public ISourceStep
{
public:
    explicit NodeScanStep(String node_variable_);

    String getName() const override { return "GraphNodeScan"; }

    QueryPlanStepPtr clone() const override;

    void initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;

private:
    static SharedHeader makeHeader(const String & node_variable);

    String node_variable;
};

}
