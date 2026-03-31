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
class GQLCallClause;
class GQLExpr;
class GQLFinishClause;
class GQLForClause;
class GQLLabelExpression;
class GQLPropertyItem;
class GQLPropertyMap;
class GQLQuantifier;
class GQLNodePattern;
class GQLEdgePattern;
class GQLPathPattern;
class GQLWhereClause;
class GQLLetClause;
class GQLMatchClause;
class GQLOrderByItem;
class GQLOrderByClause;
class GQLPageClause;
class GQLProjectClause;
class GQLSelectSourceItem;
class GQLSelectSourceList;
class GQLClausesQuery;
class GQLSetQuery;

}  // namespace DB::OPENGQL::AST
