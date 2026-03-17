#include <Parsers/graph/ASTGraphPattern.h>

#include <IO/Operators.h>

namespace DB {

ASTPtr ASTGraphPattern::clone() const {
  auto res = make_intrusive<ASTGraphPattern>(*this);
  res->children.clear();

  for (const auto& child : children) res->addPath(child->clone());

  return res;
}

void ASTGraphPattern::formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const {
  bool first = true;
  for (const auto& child : children) {
    if (!first) ostr << ", ";
    first = false;
    child->format(ostr, settings, state, frame);
  }
}

}  // namespace DB
