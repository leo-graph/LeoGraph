#pragma once

#include <Processors/ISource.h>
#include <Processors/QueryPlan/Graph/MatchSpec.h>

namespace DB::Graph
{

class MatchSource final : public ISource
{
public:
    MatchSource(SharedHeader header_, MatchSpec match_spec_);

    String getName() const override { return "GraphMatchSource"; }

protected:
    Chunk generate() override;

private:
    MatchSpec match_spec;
};

}
