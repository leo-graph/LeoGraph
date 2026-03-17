#include <Parsers/graph/ASTEdgePattern.h>

#include <IO/Operators.h>

namespace DB {

void ASTEdgePattern::setLabelExpression(const ASTPtr &child) {
  if (!child) return;

  auto *expr = child->as<ASTLabelExpression>();
  if (expr->op == GraphLabelOp::NAME)
    label = expr->label_name;
  else
    label.clear();

  setOrReplace(label_expression, child);
}

void ASTEdgePattern::setQuantifier(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(quantifier, child);
}

void ASTEdgePattern::setWherePredicate(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(where_predicate, child);
}

ASTPtr ASTEdgePattern::clone() const {
  auto res = make_intrusive<ASTEdgePattern>(*this);
  res->children.clear();
  res->label_expression = nullptr;
  res->quantifier = nullptr;
  res->where_predicate = nullptr;

  for (const auto &child : children) {
    auto cloned = child->clone();
    if (label_expression && child.get() == label_expression)
      res->setLabelExpression(cloned);
    else if (quantifier && child.get() == quantifier)
      res->setQuantifier(cloned);
    else if (where_predicate && child.get() == where_predicate)
      res->setWherePredicate(cloned);
    else
      res->children.push_back(cloned);
  }

  return res;
}

void ASTEdgePattern::formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
  const bool left_arrow = direction == GraphEdgeDirection::LEFT || direction == GraphEdgeDirection::LEFT_OR_UNDIRECTED ||
                          direction == GraphEdgeDirection::LEFT_OR_RIGHT;
  const bool right_arrow = direction == GraphEdgeDirection::RIGHT || direction == GraphEdgeDirection::UNDIRECTED_OR_RIGHT ||
                           direction == GraphEdgeDirection::LEFT_OR_RIGHT;

  if (left_arrow)
    ostr << "<-";
  else
    ostr << "-";

  ostr << "[";

  if (!variable.empty()) ostr << variable;

  if (label_expression) {
    ostr << ":";
    label_expression->format(ostr, settings, state, frame);
  } else if (!label.empty()) {
    ostr << ":" << label;
  }

  if (quantifier) quantifier->format(ostr, settings, state, frame);

  if (where_predicate) {
    ostr << " WHERE ";
    where_predicate->format(ostr, settings, state, frame);
  }

  ostr << "]";

  if (right_arrow)
    ostr << "->";
  else
    ostr << "-";
}

void ASTEdgePattern::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) {
  f(reinterpret_cast<IAST **>(&label_expression), nullptr);
  f(reinterpret_cast<IAST **>(&quantifier), nullptr);
  f(&where_predicate, nullptr);
}

}  // namespace DB
