#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLSetQuery final : public DB::IAST {
 public:
  GQLSetQuery(SetOperation operation_, Ptr left_, Ptr right_) : operation(operation_), left(std::move(left_)), right(std::move(right_)) {
    if (left) children.push_back(left);
    if (right) children.push_back(right);
  }

  String getID(char) const override { return "GQLSetQuery"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLSetQuery>(*this);
    result->children.clear();
    result->left = left ? left->clone() : Ptr{};
    result->right = right ? right->clone() : Ptr{};

    if (result->left) result->children.push_back(result->left);
    if (result->right) result->children.push_back(result->right);

    return result;
  }

  SetOperation operation;
  Ptr left;
  Ptr right;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (left) left->format(ostr, settings, state, frame);

    ostr << " " << detail::getSetOperationKeyword(operation) << " ";

    if (right) right->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &left);
    f(nullptr, &right);
  }
};

}  // namespace DB::OPENGQL::AST
