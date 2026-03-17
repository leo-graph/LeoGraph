#pragma once

#include <Parsers/graph/ASTGraphCommon.h>
#include <Parsers/graph/ASTLabelExpression.h>
#include <Parsers/graph/ASTPathQuantifier.h>

namespace DB {

class ASTEdgePattern : public IAST {
 public:
  String variable;
  String label;
  GraphEdgeDirection direction = GraphEdgeDirection::RIGHT;

  ASTLabelExpression *label_expression = nullptr;
  ASTPathQuantifier *quantifier = nullptr;
  IAST *where_predicate = nullptr;

  void setLabelExpression(const ASTPtr &child);
  void setQuantifier(const ASTPtr &child);
  void setWherePredicate(const ASTPtr &child);

  String getID(char) const override { return "EdgePattern"; }
  ASTPtr clone() const override;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override;
  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;
};

}  // namespace DB
