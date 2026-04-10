#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLLabelExpression final : public DB::IAST {
 public:
  enum class Kind : UInt8 {
    Name,
    Wildcard,
    Negation,
    Conjunction,
    Disjunction,
  };

  explicit GQLLabelExpression(Kind kind_, String text_ = {}) : kind(kind_), text(std::move(text_)) {}

  static Ptr name(const String& text) { return Ptr(make_intrusive<GQLLabelExpression>(Kind::Name, text)); }

  static Ptr wildcard() { return Ptr(make_intrusive<GQLLabelExpression>(Kind::Wildcard)); }

  static Ptr unary(const String& op, Ptr operand) {
    auto expression = make_intrusive<GQLLabelExpression>(Kind::Negation, op);
    expression->children.push_back(std::move(operand));
    return Ptr(expression);
  }

  static Ptr binary(Kind kind, Ptr left, Ptr right) {
    auto expression = make_intrusive<GQLLabelExpression>(kind);
    expression->children.push_back(std::move(left));
    expression->children.push_back(std::move(right));
    return Ptr(expression);
  }

  String getID(char delim) const override { return "GQLLabelExpression" + (delim + text); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLLabelExpression>(*this);
    result->children.clear();

    for (const auto& child : children) {
      if (child) result->children.push_back(child->clone());
    }

    return result;
  }

  Kind kind;
  String text;

 protected:
  void formatImpl(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state, FormatStateStacked frame) const override {
    switch (kind) {
      case Kind::Name:
        ostr << text;
        return;

      case Kind::Wildcard:
        ostr << "%";
        return;

      case Kind::Negation:
        ostr << "!";

        if (!children.empty() && children.front()) children.front()->format(ostr, settings, state, frame);

        return;

      case Kind::Conjunction:
      case Kind::Disjunction: {
        const char* separator = kind == Kind::Conjunction ? " & " : " | ";
        ostr << "(";

        if (!children.empty() && children[0]) children[0]->format(ostr, settings, state, frame);

        ostr << separator;

        if (children.size() > 1 && children[1]) children[1]->format(ostr, settings, state, frame);

        ostr << ")";
        return;
      }
    }
  }
};

}  // namespace DB::OPENGQL::AST
