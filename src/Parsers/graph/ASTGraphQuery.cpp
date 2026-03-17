#include <Parsers/graph/ASTGraphQuery.h>

#include <IO/Operators.h>

namespace DB {

void ASTGraphQuery::setMatchPattern(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(match_pattern, child);
}

void ASTGraphQuery::setWhereCondition(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(where_condition, child);
}

void ASTGraphQuery::setReturnClause(const ASTPtr &child) {
  if (!child) return;

  setOrReplace(return_clause, child);
}

ASTPtr ASTGraphQuery::clone() const {
  auto res = make_intrusive<ASTGraphQuery>(*this);
  res->children.clear();
  res->match_pattern = nullptr;
  res->where_condition = nullptr;
  res->return_clause = nullptr;

  for (const auto &child : children) {
    auto cloned = child->clone();
    if (match_pattern && child.get() == match_pattern)
      res->setMatchPattern(cloned);
    else if (where_condition && child.get() == where_condition)
      res->setWhereCondition(cloned);
    else if (return_clause && child.get() == return_clause)
      res->setReturnClause(cloned);
    else
      res->children.push_back(cloned);
  }

  return res;
}

void ASTGraphQuery::formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const {
  if (is_optional_match) ostr << "OPTIONAL ";

  ostr << "MATCH ";

  if (match_pattern) match_pattern->format(ostr, settings, state, frame);

  if (where_condition) {
    ostr << " WHERE ";
    where_condition->format(ostr, settings, state, frame);
  }

  if (return_clause) {
    ostr << " ";
    return_clause->format(ostr, settings, state, frame);
  }
}

void ASTGraphQuery::forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) {
  f(reinterpret_cast<IAST **>(&match_pattern), nullptr);
  f(&where_condition, nullptr);
  f(reinterpret_cast<IAST **>(&return_clause), nullptr);
}
}  // namespace DB
