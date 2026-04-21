#pragma once

#include <Parsers/ASTWithAlias.h>
#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLExpr final : public DB::ASTWithAlias {
 public:
  enum class SetQuantifier : UInt8 {
    None,
    Distinct,
    All,
  };

  enum class TrimSpec : UInt8 {
    None,
    Leading,
    Trailing,
    Both,
  };

  enum class TemporalQualifier : UInt8 {
    None,
    YearToMonth,
    DayToSecond,
  };

  enum class Kind : UInt8 {
    Identifier,
    Literal,
    Property,
    UnaryOp,
    BinaryOp,
    FunctionCall,
    Cast,
    DurationBetween,
    TrimString,
    ExprList,
    RawText,
  };

  explicit GQLExpr(Kind kind_, String text_ = {}) : kind(kind_), text(std::move(text_)) {}

  static Ptr identifier(const String& name) { return Ptr(make_intrusive<GQLExpr>(Kind::Identifier, name)); }

  static Ptr literal(const String& text) { return Ptr(make_intrusive<GQLExpr>(Kind::Literal, text)); }

  static Ptr property(Ptr base, const String& property_name) {
    auto expression = make_intrusive<GQLExpr>(Kind::Property, property_name);
    expression->children.push_back(std::move(base));
    return Ptr(expression);
  }

  static Ptr unaryOp(const String& op, Ptr operand) {
    auto expression = make_intrusive<GQLExpr>(Kind::UnaryOp, op);
    expression->children.push_back(std::move(operand));
    return Ptr(expression);
  }

  static Ptr binaryOp(const String& op, Ptr left, Ptr right) {
    auto expression = make_intrusive<GQLExpr>(Kind::BinaryOp, op);
    expression->children.push_back(std::move(left));
    expression->children.push_back(std::move(right));
    return Ptr(expression);
  }

  static Ptr functionCall(const String& name, PtrList arguments, SetQuantifier set_quantifier_ = SetQuantifier::None) {
    auto expression = make_intrusive<GQLExpr>(Kind::FunctionCall, name);
    expression->set_quantifier = set_quantifier_;
    expression->children = std::move(arguments);
    return Ptr(expression);
  }

  static Ptr castExpr(Ptr operand, const String& target_type) {
    auto expression = make_intrusive<GQLExpr>(Kind::Cast, target_type);
    expression->children.push_back(std::move(operand));
    return Ptr(expression);
  }

  static Ptr trimString(Ptr source, TrimSpec spec, Ptr trim_char = nullptr) {
    auto expression = make_intrusive<GQLExpr>(Kind::TrimString);
    expression->trim_spec = spec;
    expression->children.push_back(std::move(source));
    if (trim_char) expression->children.push_back(std::move(trim_char));
    return Ptr(expression);
  }

  static Ptr durationBetween(Ptr left, Ptr right, TemporalQualifier qualifier = TemporalQualifier::None) {
    auto expression = make_intrusive<GQLExpr>(Kind::DurationBetween, "DURATION_BETWEEN");
    expression->temporal_qualifier = qualifier;
    expression->children.push_back(std::move(left));
    expression->children.push_back(std::move(right));
    return Ptr(expression);
  }

  static Ptr exprList(PtrList items) {
    auto expression = make_intrusive<GQLExpr>(Kind::ExprList);
    expression->children = std::move(items);
    return Ptr(expression);
  }

  static Ptr rawText(const String& text) { return Ptr(make_intrusive<GQLExpr>(Kind::RawText, text)); }

  String getID(char delim) const override { return "GQLExpr" + (delim + text); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLExpr>(*this);
    result->children.clear();

    for (const auto& child : children) {
      if (child) result->children.push_back(child->clone());
    }

    return result;
  }

  Kind kind;
  String text;
  SetQuantifier set_quantifier = SetQuantifier::None;
  TrimSpec trim_spec = TrimSpec::None;
  TemporalQualifier temporal_qualifier = TemporalQualifier::None;

 protected:
  void formatImplWithoutAlias(WriteBuffer& ostr, const FormatSettings& settings, FormatState& state,
                              FormatStateStacked frame) const override {
    switch (kind) {
      case Kind::Identifier:
      case Kind::Literal:
      case Kind::RawText:
        ostr << text;
        return;

      case Kind::Property: {
        if (!children.empty() && children.front()) children.front()->format(ostr, settings, state, frame);

        ostr << "." << text;
        return;
      }

      case Kind::UnaryOp: {
        ostr << text;

        if (!children.empty() && children.front()) children.front()->format(ostr, settings, state, frame);

        return;
      }

      case Kind::BinaryOp: {
        ostr << "(";

        if (!children.empty() && children[0]) children[0]->format(ostr, settings, state, frame);

        ostr << " " << text << " ";

        if (children.size() > 1 && children[1]) children[1]->format(ostr, settings, state, frame);

        ostr << ")";
        return;
      }

      case Kind::FunctionCall: {
        ostr << text << "(";
        if (set_quantifier != SetQuantifier::None) {
          ostr << (set_quantifier == SetQuantifier::Distinct ? "DISTINCT" : "ALL");
          if (!children.empty()) ostr << " ";
        }
        detail::formatChildren(ostr, settings, state, frame, children, ", ");
        ostr << ")";
        return;
      }

      case Kind::Cast: {
        ostr << "CAST(";
        if (!children.empty() && children.front()) children.front()->format(ostr, settings, state, frame);
        ostr << " AS " << text << ")";
        return;
      }

      case Kind::TrimString: {
        ostr << "TRIM(";
        bool has_prefix = (trim_spec != TrimSpec::None) || children.size() > 1;
        if (has_prefix) {
          if (trim_spec == TrimSpec::Leading)
            ostr << "LEADING";
          else if (trim_spec == TrimSpec::Trailing)
            ostr << "TRAILING";
          else if (trim_spec == TrimSpec::Both)
            ostr << "BOTH";
          if (children.size() > 1 && children[1]) {
            if (trim_spec != TrimSpec::None) ostr << " ";
            children[1]->format(ostr, settings, state, frame);
          }
          ostr << " FROM ";
        }
        if (!children.empty() && children[0]) children[0]->format(ostr, settings, state, frame);
        ostr << ")";
        return;
      }

      case Kind::ExprList: {
        detail::formatChildren(ostr, settings, state, frame, children, ", ");
        return;
      }

      case Kind::DurationBetween: {
        ostr << "DURATION_BETWEEN(";
        if (!children.empty() && children[0]) children[0]->format(ostr, settings, state, frame);
        ostr << ", ";
        if (children.size() > 1 && children[1]) children[1]->format(ostr, settings, state, frame);
        ostr << ")";
        if (temporal_qualifier == TemporalQualifier::YearToMonth)
          ostr << " YEAR TO MONTH";
        else if (temporal_qualifier == TemporalQualifier::DayToSecond)
          ostr << " DAY TO SECOND";
        return;
      }
    }
  }

  void appendColumnNameImpl(WriteBuffer& ostr) const override { ostr << detail::formatNodeToString(*this); }
};

}  // namespace DB::OPENGQL::AST
