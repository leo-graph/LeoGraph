#pragma once

#include <Processors/QueryPlan/Optimizations/QueryPlanOptimizationSettings.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB {

namespace QueryPlanOptimizations {

/// Heuristic-based algorithm to decide whether to enable parallel replicas for the given query.
void considerEnablingParallelReplicas(const QueryPlanOptimizationSettings& optimization_settings, QueryPlan::Node& root,
                                      QueryPlan& query_plan);

}  // namespace QueryPlanOptimizations

}  // namespace DB
