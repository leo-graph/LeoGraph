#include <Processors/QueryPlan/Optimizations/QueryPlanOptimizationSettings.h>
#include <Processors/QueryPlan/QueryPlan.h>

namespace DB::QueryPlanOptimizations {

void tryMakeDistributedJoin(QueryPlan::Node &, QueryPlan::Nodes &, const QueryPlanOptimizationSettings &) {}
void tryMakeDistributedAggregation(QueryPlan::Node &, QueryPlan::Nodes &, const QueryPlanOptimizationSettings &) {}
void tryMakeDistributedSorting(QueryPlan::Node &, QueryPlan::Nodes &, const QueryPlanOptimizationSettings &) {}
void tryMakeDistributedRead(QueryPlan::Node &, QueryPlan::Nodes &, const QueryPlanOptimizationSettings &) {}
void optimizeExchanges(QueryPlan::Node &) {}

}  // namespace DB::QueryPlanOptimizations
