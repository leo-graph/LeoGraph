#pragma once

// GQL QueryTree Nodes - Convenience header that includes all GQL node types

// Query root nodes
#include <Analyzer/GQL/GQLLinearQueryNode.h>
#include <Analyzer/GQL/GQLCombinedQueryNode.h>

// Match and optional nodes
#include <Analyzer/GQL/GQLMatchNode.h>
#include <Analyzer/GQL/GQLOptionalBlockNode.h>

// Path pattern nodes
#include <Analyzer/GQL/GQLPathPatternNode.h>
#include <Analyzer/GQL/GQLPathTermNode.h>
#include <Analyzer/GQL/GQLPathAlternationNode.h>
#include <Analyzer/GQL/GQLSimplifiedPathPatternNode.h>
#include <Analyzer/GQL/GQLSimplifiedPathExprNode.h>

// Element pattern nodes
#include <Analyzer/GQL/GQLNodePatternNode.h>
#include <Analyzer/GQL/GQLEdgePatternNode.h>
#include <Analyzer/GQL/GQLQuantifierNode.h>

// Constraint nodes
#include <Analyzer/GQL/GQLLabelExpressionNode.h>
#include <Analyzer/GQL/GQLPropertyMapNode.h>
#include <Analyzer/GQL/GQLPropertyItemNode.h>

// Projection and filter nodes
#include <Analyzer/GQL/GQLKeepNode.h>
#include <Analyzer/GQL/GQLYieldNode.h>
#include <Analyzer/GQL/GQLReturnNode.h>
#include <Analyzer/GQL/GQLFilterNode.h>

// Post-processing nodes
#include <Analyzer/GQL/GQLOrderByNode.h>
#include <Analyzer/GQL/GQLPageNode.h>
