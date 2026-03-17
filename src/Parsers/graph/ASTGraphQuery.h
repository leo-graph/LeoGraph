#pragma once

#include <Parsers/graph/ASTEdgePattern.h>
#include <Parsers/graph/ASTGraphCommon.h>
#include <Parsers/graph/ASTGraphPattern.h>
#include <Parsers/graph/ASTGraphReturnClause.h>
#include <Parsers/graph/ASTGraphReturnItem.h>
#include <Parsers/graph/ASTLabelExpression.h>
#include <Parsers/graph/ASTNodePattern.h>
#include <Parsers/graph/ASTPathPattern.h>
#include <Parsers/graph/ASTPathQuantifier.h>

namespace DB {

class ASTGraphQuery : public IAST {
 public:
  String graph_name;
  bool is_optional_match = false;

  ASTGraphPattern *match_pattern = nullptr;
  IAST *where_condition = nullptr;
  ASTGraphReturnClause *return_clause = nullptr;

  void setMatchPattern(const ASTPtr &child);
  void setWhereCondition(const ASTPtr &child);
  void setReturnClause(const ASTPtr &child);

  String getID(char) const override { return "GraphQuery"; }
  ASTPtr clone() const override;

  QueryKind getQueryKind() const override { return QueryKind::Select; }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override;
  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;
};
}  // namespace DB
