#pragma once

#include <Parsers/ASTWithAlias.h>
#include <Parsers/graph/AST/GQLExpr.h>
#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

/// Represents a GQL CASE expression in both simple and searched forms.
///
/// Simple:   CASE operand WHEN v1 THEN r1 [WHEN v2 THEN r2 ...] [ELSE default] END
/// Searched: CASE WHEN c1 THEN r1 [WHEN c2 THEN r2 ...] [ELSE default] END
class GQLCaseExpr final : public DB::ASTWithAlias {
 public:
  enum class Form : UInt8 {
    Simple,
    Searched,
  };

  explicit GQLCaseExpr(Form form_) : form(form_) {}

  String getID(char) const override { return "GQLCaseExpr"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLCaseExpr>(form);
    result->operand = operand ? operand->clone() : Ptr{};
    result->else_result = else_result ? else_result->clone() : Ptr{};

    result->when_operands.reserve(when_operands.size());
    result->then_results.reserve(then_results.size());

    if (result->operand) result->children.push_back(result->operand);

    for (size_t i = 0; i < when_operands.size(); ++i) {
      auto w = when_operands[i] ? when_operands[i]->clone() : Ptr{};
      auto t = (i < then_results.size() && then_results[i]) ? then_results[i]->clone() : Ptr{};
      result->when_operands.push_back(w);
      result->then_results.push_back(t);
      if (w) result->children.push_back(w);
      if (t) result->children.push_back(t);
    }

    if (result->else_result) result->children.push_back(result->else_result);

    return result;
  }

  Form form;
  Ptr operand;  /// only for Form::Simple
  PtrList when_operands;
  PtrList then_results;
  Ptr else_result;  /// nullable

 protected:
  void formatImplWithoutAlias(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state,
                              FormatStateStacked frame) const override {
    ostr << "CASE";

    if (form == Form::Simple && operand) {
      ostr << " ";
      operand->format(ostr, settings, state, frame);
    }

    for (size_t i = 0; i < when_operands.size(); ++i) {
      ostr << " WHEN ";
      if (when_operands[i]) {
        auto* when_expr = when_operands[i]->as<GQLExpr>();
        if (form == Form::Simple && when_expr && when_expr->kind == GQLExpr::Kind::BinaryOp && when_expr->children.size() > 1) {
          ostr << when_expr->text << " ";
          when_expr->children[1]->format(ostr, settings, state, frame);
        } else {
          when_operands[i]->format(ostr, settings, state, frame);
        }
      }
      ostr << " THEN ";
      if (i < then_results.size() && then_results[i]) then_results[i]->format(ostr, settings, state, frame);
    }

    if (else_result) {
      ostr << " ELSE ";
      else_result->format(ostr, settings, state, frame);
    }

    ostr << " END";
  }

  void appendColumnNameImpl(WriteBuffer& ostr) const override { ostr << detail::formatNodeToString(*this); }
};

}  // namespace DB::OPENGQL::AST
