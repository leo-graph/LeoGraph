#pragma once

#include <Parsers/ASTWithAlias.h>
#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLExpr final : public DB::ASTWithAlias {
 public:
  enum class Kind : UInt8 {
    Identifier,
    Literal,
    Property,
    UnaryOp,
    BinaryOp,
    FunctionCall,
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

  static Ptr functionCall(const String& name, PtrList arguments) {
    auto expression = make_intrusive<GQLExpr>(Kind::FunctionCall, name);
    expression->children = std::move(arguments);
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
        detail::formatChildren(ostr, settings, state, frame, children, ", ");
        ostr << ")";
        return;
      }
    }
  }

  void appendColumnNameImpl(WriteBuffer& ostr) const override { ostr << detail::formatNodeToString(*this); }
};

}  // namespace DB::OPENGQL::AST
