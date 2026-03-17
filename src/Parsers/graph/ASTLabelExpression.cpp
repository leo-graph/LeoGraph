#include <Parsers/graph/ASTLabelExpression.h>

#include <IO/Operators.h>

namespace DB {

ASTPtr ASTLabelExpression::clone() const {
  auto res = make_intrusive<ASTLabelExpression>(*this);
  res->children.clear();

  for (const auto& child : children) res->addArgument(child->clone());

  return res;
}

void ASTLabelExpression::formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
  switch (op) {
    case GraphLabelOp::NAME:
      ostr << label_name;
      break;
    case GraphLabelOp::WILDCARD:
      ostr << "%";
      break;
    case GraphLabelOp::NEGATION:
      ostr << "!";
      if (!children.empty()) children[0]->format(ostr, settings, state, frame);
      break;
    case GraphLabelOp::CONJUNCTION:
      if (children.size() >= 2) {
        children[0]->format(ostr, settings, state, frame);
        ostr << "&";
        children[1]->format(ostr, settings, state, frame);
      }
      break;
    case GraphLabelOp::DISJUNCTION:
      if (children.size() >= 2) {
        children[0]->format(ostr, settings, state, frame);
        ostr << "|";
        children[1]->format(ostr, settings, state, frame);
      }
      break;
  }
}

}  // namespace DB
