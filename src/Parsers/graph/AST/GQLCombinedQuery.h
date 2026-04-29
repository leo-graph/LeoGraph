#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLCombinedQuery final : public DB::IAST {
 public:
  GQLCombinedQuery() = default;

  GQLCombinedQuery(PtrList queries_, std::vector<CombinedQueryOperator> operators_)
      : queries(std::move(queries_)), operators(std::move(operators_)) {
    for (const auto &query : queries) {
      if (query) children.push_back(query);
    }
  }

  String getID(char) const override { return "GQLCombinedQuery"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLCombinedQuery>(*this);
    result->children.clear();
    detail::cloneChildrenList(queries, result->queries, result->children);
    return result;
  }

  PtrList queries;
  std::vector<CombinedQueryOperator> operators;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    const size_t query_count = queries.size();

    for (size_t i = 0; i < query_count; ++i) {
      if (i > 0) {
        auto operation = CombinedQueryOperator::UnionDistinct;

        if (i - 1 < operators.size()) operation = operators[i - 1];

        ostr << " " << detail::getCombinedQueryOperatorKeyword(operation) << " ";
      }

      if (queries[i]) queries[i]->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &query : queries) f(nullptr, &query);
  }
};

}  // namespace DB::OPENGQL::AST
