#pragma once

#include <Analyzer/FunctionNode.h>
#include <Analyzer/IQueryTreeNode.h>

#include <Planner/PlannerActionsVisitor.h>
#include <Planner/PlannerContext.h>

#include <Processors/QueryPlan/AggregatingStep.h>

namespace DB {

/// Extract aggregate descriptions from aggregate function nodes
AggregateDescriptions extractAggregateDescriptions(const QueryTreeNodes& aggregate_function_nodes, const PlannerContext& planner_context);

}  // namespace DB
