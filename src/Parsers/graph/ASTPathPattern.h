#pragma once

#include <Parsers/graph/ASTGraphCommon.h>
#include <Parsers/graph/ASTPathQuantifier.h>

namespace DB {

class ASTPathPattern : public IAST {
 public:
  GraphPathMode path_mode = GraphPathMode::DEFAULT;
  GraphPathSearch search_prefix = GraphPathSearch::NONE;
  uint64_t search_count = 0;
  String path_variable;
  String subpath_variable;

  ASTPathQuantifier *quantifier = nullptr;
  IAST *where_predicate = nullptr;

  void addElement(const ASTPtr &child) {
    if (child) children.push_back(child);
  }

  void setQuantifier(const ASTPtr &child);
  void setWherePredicate(const ASTPtr &child);

  String getID(char) const override { return "PathPattern"; }
  ASTPtr clone() const override;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override;
  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;
};

}  // namespace DB
