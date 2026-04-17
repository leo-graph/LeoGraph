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

enum class CombinedQueryOperator : UInt8 {
  UnionDistinct,
  UnionAll,
  ExceptDistinct,
  ExceptAll,
  IntersectDistinct,
  IntersectAll,
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
class GQLCombinedQuery;
class GQLExpr;
class GQLFinishClause;
class GQLForClause;
class GQLGroupByClause;
class GQLInlineCallClause;
class GQLLabelExpression;
class GQLListConstructor;
class GQLNamedCallClause;
class GQLPropertyItem;
class GQLPropertyMap;
class GQLQuantifier;
class GQLRecordConstructor;
class GQLReturnClause;
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
class GQLSchemaReference;
class GQLSelectClause;
class GQLSelectSourceItem;
class GQLSelectSourceList;
class GQLSubquery;
class GQLSubqueryNextClause;
class GQLSingleQuery;
class GQLUseClause;
class GQLYieldClause;

}  // namespace DB::OPENGQL::AST
