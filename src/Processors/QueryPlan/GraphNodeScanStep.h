#pragma once

#include <Processors/QueryPlan/ISourceStep.h>

namespace DB
{

class GraphNodeScanStep final : public ISourceStep
{
public:
    explicit GraphNodeScanStep(String node_variable_);

    String getName() const override { return "GraphNodeScan"; }

    QueryPlanStepPtr clone() const override;

    void initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;

private:
    static SharedHeader makeHeader(const String & node_variable);

    String node_variable;
};

}
