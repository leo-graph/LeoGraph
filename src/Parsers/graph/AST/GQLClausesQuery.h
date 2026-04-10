#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLClausesQuery final : public DB::IAST {
 public:
  GQLClausesQuery() = default;

  explicit GQLClausesQuery(PtrList clauses_) : clauses(std::move(clauses_)) {
    for (const auto &clause : clauses) {
      if (clause) children.push_back(clause);
    }
  }

  String getID(char) const override { return "GQLClausesQuery"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLClausesQuery>(*this);
    result->children.clear();
    detail::cloneChildrenList(clauses, result->clauses, result->children);
    return result;
  }

  PtrList clauses;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    detail::formatChildren(ostr, settings, state, frame, clauses, " ");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &clause : clauses) f(nullptr, &clause);
  }
};

}  // namespace DB::OPENGQL::AST
