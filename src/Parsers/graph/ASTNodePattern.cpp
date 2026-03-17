#include <Parsers/graph/ASTNodePattern.h>

#include <IO/Operators.h>

namespace DB {

void ASTNodePattern::setLabelExpression(const ASTPtr &child) {
  if (!child) return;

  auto *expr = child->as<ASTLabelExpression>();
  if (expr->op == GraphLabelOp::NAME)
    label = expr->label_name;
  else
    label.clear();

  setOrReplace(label_expression, child);
}

void ASTNodePattern::setWherePredicate(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(where_predicate, child);
}

ASTPtr ASTNodePattern::clone() const {
  auto res = make_intrusive<ASTNodePattern>(*this);
  res->children.clear();
  res->label_expression = nullptr;
  res->where_predicate = nullptr;

  for (const auto &child : children) {
    auto cloned = child->clone();
    if (label_expression && child.get() == label_expression)
      res->setLabelExpression(cloned);
    else if (where_predicate && child.get() == where_predicate)
      res->setWherePredicate(cloned);
    else
      res->children.push_back(cloned);
  }

  return res;
}

void ASTNodePattern::formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
  ostr << "(";

  if (!variable.empty()) ostr << variable;

  if (label_expression) {
    ostr << ":";
    label_expression->format(ostr, settings, state, frame);
  } else if (!label.empty()) {
    ostr << ":" << label;
  }

  if (where_predicate) {
    ostr << " WHERE ";
    where_predicate->format(ostr, settings, state, frame);
  }

  ostr << ")";
}

void ASTNodePattern::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) {
  f(reinterpret_cast<IAST **>(&label_expression), nullptr);
  f(&where_predicate, nullptr);
}

}  // namespace DB
