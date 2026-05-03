#pragma once

#include <Interpreters/GQL/PatternLowering.h>
#include <Processors/QueryPlan/Graph/MatchSpec.h>

#include <vector>

namespace DB::GQL
{

Graph::MatchSpec makeMatchSpec(const MatchPlan & match);
Graph::MatchSpec makeMatchSpec(const std::vector<MatchPlan> & matches);

}
