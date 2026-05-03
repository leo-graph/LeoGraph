#pragma once

#include <Processors/QueryPlan/Graph/MatchSpec.h>
#include <Processors/QueryPlan/ISourceStep.h>

#include <memory>

namespace DB::Graph
{

class IMatchSourceFactory;
using MatchSourceFactoryPtr = std::shared_ptr<const IMatchSourceFactory>;

class MatchStep final : public ISourceStep
{
public:
    explicit MatchStep(MatchSpec match_spec_, MatchSourceFactoryPtr source_factory_ = {});

    String getName() const override { return "GraphMatch"; }

    QueryPlanStepPtr clone() const override;

    void initializePipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;

    const MatchSpec & getMatchSpec() const { return match_spec; }
    const MatchSourceFactoryPtr & getSourceFactory() const { return source_factory; }

private:
    static SharedHeader makeHeader(const MatchSpec & match_spec);

    MatchSpec match_spec;
    MatchSourceFactoryPtr source_factory;
};

}
