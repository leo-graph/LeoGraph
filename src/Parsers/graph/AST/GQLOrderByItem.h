#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLOrderByItem final : public DB::IAST {
 public:
  enum class NullOrdering { Unspecified, NullsFirst, NullsLast };

  GQLOrderByItem(Ptr expression_, bool descending_, NullOrdering null_ordering_ = NullOrdering::Unspecified)
      : expression(std::move(expression_)), descending(descending_), null_ordering(null_ordering_) {
    if (expression) children.push_back(expression);
  }

  String getID(char) const override { return "GQLOrderByItem"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLOrderByItem>(*this);
    result->children.clear();
    result->expression = expression ? expression->clone() : Ptr{};

    if (result->expression) result->children.push_back(result->expression);

    return result;
  }

  Ptr expression;
  bool descending;
  NullOrdering null_ordering;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (expression) expression->format(ostr, settings, state, frame);

    if (descending) ostr << " DESC";

    if (null_ordering == NullOrdering::NullsFirst) {
      ostr << " NULLS FIRST";
    } else if (null_ordering == NullOrdering::NullsLast) {
      ostr << " NULLS LAST";
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &expression); }
};

}  // namespace DB::OPENGQL::AST
