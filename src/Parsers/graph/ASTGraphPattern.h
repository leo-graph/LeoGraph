#pragma once

#include <Parsers/IAST.h>

namespace DB {

class ASTGraphPattern : public IAST {
 public:
  void addPath(const ASTPtr& child) {
    if (child) children.push_back(child);
  }

  String getID(char) const override { return "GraphPattern"; }
  ASTPtr clone() const override;

 protected:
  void formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const override;
};

}  // namespace DB
