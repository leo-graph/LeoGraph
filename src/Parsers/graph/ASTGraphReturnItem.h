#pragma once

#include <Parsers/ASTWithAlias.h>

namespace DB {

class ASTGraphReturnItem : public ASTWithAlias {
 public:
  void setExpression(const ASTPtr &child);
  ASTPtr getExpression() const { return expression ? expression->ptr() : ASTPtr{}; }

  String getID(char) const override { return "GraphReturnItem"; }
  ASTPtr clone() const override;

 protected:
  void formatImplWithoutAlias(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state,
                              FormatStateStacked frame) const override;
  void appendColumnNameImpl(WriteBuffer &ostr) const override;
  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override;

 private:
  IAST *expression = nullptr;
};

}  // namespace DB
