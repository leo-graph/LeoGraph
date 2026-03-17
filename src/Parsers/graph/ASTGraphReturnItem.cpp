#include <Parsers/graph/ASTGraphReturnItem.h>

namespace DB {

void ASTGraphReturnItem::setExpression(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(expression, child);
}

ASTPtr ASTGraphReturnItem::clone() const {
  auto res = make_intrusive<ASTGraphReturnItem>(*this);
  res->children.clear();
  res->expression = nullptr;

  if (expression) res->setExpression(expression->clone());

  return res;
}

void ASTGraphReturnItem::formatImplWithoutAlias(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state,
                                                FormatStateStacked frame) const {
  if (expression) expression->format(ostr, settings, state, frame);
}

void ASTGraphReturnItem::appendColumnNameImpl(WriteBuffer &ostr) const {
  if (expression) expression->appendColumnName(ostr);
}

void ASTGraphReturnItem::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) { f(&expression, nullptr); }

}  // namespace DB
