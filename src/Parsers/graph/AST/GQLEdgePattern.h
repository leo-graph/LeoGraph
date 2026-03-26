#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLEdgePattern final : public DB::IAST {
 public:
  explicit GQLEdgePattern(EdgeDirection direction_) : direction(direction_) {}

  String getID(char) const override { return "GQLEdgePattern"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLEdgePattern>(*this);
    result->children.clear();
    result->variable = variable ? variable->clone() : Ptr{};
    result->label_expression = label_expression ? label_expression->clone() : Ptr{};
    result->properties = properties ? properties->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};
    result->quantifier = quantifier ? quantifier->clone() : Ptr{};

    if (result->variable) result->children.push_back(result->variable);
    if (result->label_expression) result->children.push_back(result->label_expression);
    if (result->properties) result->children.push_back(result->properties);
    if (result->where) result->children.push_back(result->where);
    if (result->quantifier) result->children.push_back(result->quantifier);

    return result;
  }

  EdgeDirection direction;
  Ptr variable;
  Ptr label_expression;
  Ptr properties;
  Ptr where;
  Ptr quantifier;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    detail::formatEdgePrefix(ostr, direction);

    if (variable) variable->format(ostr, settings, state, frame);

    if (label_expression) {
      ostr << ":";
      label_expression->format(ostr, settings, state, frame);
    }

    if (properties) {
      ostr << " ";
      properties->format(ostr, settings, state, frame);
    }

    if (where) {
      ostr << " ";
      where->format(ostr, settings, state, frame);
    }

    ostr << "]";
    detail::formatEdgeSuffix(ostr, direction);

    if (quantifier) quantifier->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &variable);
    f(nullptr, &label_expression);
    f(nullptr, &properties);
    f(nullptr, &where);
    f(nullptr, &quantifier);
  }
};

}  // namespace DB::OPENGQL::AST
