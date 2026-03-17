#include <Parsers/graph/ASTPathPattern.h>

#include <IO/Operators.h>

namespace DB {

void ASTPathPattern::setQuantifier(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(quantifier, child);
}

void ASTPathPattern::setWherePredicate(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(where_predicate, child);
}

ASTPtr ASTPathPattern::clone() const {
  auto res = make_intrusive<ASTPathPattern>(*this);
  res->children.clear();
  res->quantifier = nullptr;
  res->where_predicate = nullptr;

  for (const auto &child : children) {
    auto cloned = child->clone();
    if (quantifier && child.get() == quantifier)
      res->setQuantifier(cloned);
    else if (where_predicate && child.get() == where_predicate)
      res->setWherePredicate(cloned);
    else
      res->addElement(cloned);
  }

  return res;
}

void ASTPathPattern::formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
  if (search_prefix != GraphPathSearch::NONE) {
    switch (search_prefix) {
      case GraphPathSearch::ALL:
        ostr << "ALL ";
        break;
      case GraphPathSearch::ANY:
        ostr << "ANY ";
        if (search_count > 0) ostr << search_count << " ";
        break;
      case GraphPathSearch::SHORTEST:
        ostr << "SHORTEST ";
        if (search_count > 0) ostr << search_count << " ";
        break;
      case GraphPathSearch::ALL_SHORTEST:
        ostr << "ALL SHORTEST ";
        break;
      case GraphPathSearch::ANY_SHORTEST:
        ostr << "ANY SHORTEST ";
        break;
      default:
        break;
    }
  }

  if (path_mode != GraphPathMode::DEFAULT) {
    switch (path_mode) {
      case GraphPathMode::WALK:
        ostr << "WALK ";
        break;
      case GraphPathMode::TRAIL:
        ostr << "TRAIL ";
        break;
      case GraphPathMode::SIMPLE:
        ostr << "SIMPLE ";
        break;
      case GraphPathMode::ACYCLIC:
        ostr << "ACYCLIC ";
        break;
      default:
        break;
    }
  }

  if (!path_variable.empty()) ostr << path_variable << " = ";

  for (const auto &child : children) {
    if (child.get() != where_predicate) child->format(ostr, settings, state, frame);
  }

  if (where_predicate) {
    ostr << " WHERE ";
    where_predicate->format(ostr, settings, state, frame);
  }
}

void ASTPathPattern::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) {
  f(reinterpret_cast<IAST **>(&quantifier), nullptr);
  f(&where_predicate, nullptr);
}

}  // namespace DB
