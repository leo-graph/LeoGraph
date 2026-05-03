#pragma once

#include <Processors/QueryPlan/Graph/MatchSpec.h>
#include <Processors/QueryPlan/ISourceStep.h>

namespace DB::Graph
{

class MatchStep final : public ISourceStep
{
public:
    explicit MatchStep(MatchSpec match_spec_);

    String getName() const override { return "GraphMatch"; }

    QueryPlanStepPtr clone() const override;

    void initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;

    const MatchSpec & getMatchSpec() const { return match_spec; }

private:
    static SharedHeader makeHeader(const MatchSpec & match_spec);

    MatchSpec match_spec;
};

}
