#pragma once

#include <base/types.h>
#include <Parsers/IAST_fwd.h>

namespace DB::OPENGQL::AST {

enum class EdgeDirection : UInt8 {
  Left,
  Right,
  Undirected,
  LeftOrRight,
  LeftOrUndirected,
  UndirectedOrRight,
  Any,
};

enum class GraphMatchMode : UInt8 {
  None,
  RepeatableElements,
  RepeatableElementBindings,
  DifferentEdges,
  DifferentEdgeBindings,
};

enum class PathMode : UInt8 {
  None,
  Walk,
  Trail,
  Simple,
  Acyclic,
};

enum class PathSearchKind : UInt8 {
  None,
  All,
  Any,
  AllShortest,
  AnyShortest,
  CountedShortest,
  CountedShortestGroup,
};

enum class SetOperation : UInt8 {
  Union,
  Except,
  Intersect,
  Otherwise,
};

using Ptr = DB::ASTPtr;
using PtrList = DB::ASTs;

template <typename T>
using PtrTo = boost::intrusive_ptr<T>;

class GQLAssignmentItem;
class GQLAtSchemaClause;
class GQLBindingTableExpression;
class GQLBindingInitializer;
class GQLBindingVariableDefinition;
class GQLBindingVariableDefinitionBlock;
class GQLCallClause;
class GQLExpr;
class GQLFinishClause;
class GQLForClause;
class GQLGroupByClause;
class GQLLabelExpression;
class GQLListConstructor;
class GQLPropertyItem;
class GQLPropertyMap;
class GQLQuantifier;
class GQLRecordConstructor;
class GQLNodePattern;
class GQLEdgePattern;
class GQLPathPattern;
class GQLPathPatternPrefix;
class GQLParenthesizedPathPattern;
class GQLGraphPatternBlock;
class GQLGraphExpression;
class GQLMatchStatementBlock;
class GQLWhereClause;
class GQLLetClause;
class GQLMatchClause;
class GQLOrderByItem;
class GQLOrderByClause;
class GQLPageClause;
class GQLProjectClause;
class GQLSchemaReference;
class GQLSelectSourceItem;
class GQLSelectSourceList;
class GQLSubqueryClause;
class GQLSubqueryNextClause;
class GQLUseClause;
class GQLYieldClause;
class GQLClausesQuery;
class GQLSetQuery;

}  // namespace DB::OPENGQL::AST
