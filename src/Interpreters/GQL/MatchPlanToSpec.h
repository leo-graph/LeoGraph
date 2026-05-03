#pragma once

#include <Interpreters/GQL/PatternLowering.h>
#include <Processors/QueryPlan/Graph/MatchSpec.h>

namespace DB::GQL
{

Graph::MatchSpec makeMatchSpec(const MatchPlan & match);

}
