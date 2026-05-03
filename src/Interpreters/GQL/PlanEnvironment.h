#pragma once

#include <memory>

namespace DB::Graph
{
class IMatchSourceFactory;
using MatchSourceFactoryPtr = std::shared_ptr<const IMatchSourceFactory>;
}

namespace DB::GQL
{

/// Planner-wide services shared by a query and its nested query builders.
/// This is intentionally separate from PlanScope, which only tracks lexical bindings and active graph scope.
struct PlanEnvironment
{
    Graph::MatchSourceFactoryPtr match_source_factory;
};

}
