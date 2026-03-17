#pragma once

#include <Parsers/graph/ASTGraphCommon.h>
#include <Parsers/IAST.h>

namespace DB {

class ASTLabelExpression : public IAST {
 public:
  GraphLabelOp op = GraphLabelOp::NAME;
  String label_name;

  void addArgument(const ASTPtr& child) {
    if (child) children.push_back(child);
  }

  String getID(char) const override { return "LabelExpression"; }
  ASTPtr clone() const override;

 protected:
  void formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const override;
};

}  // namespace DB
