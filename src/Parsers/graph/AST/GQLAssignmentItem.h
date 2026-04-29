#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLAssignmentItem final : public DB::IAST {
 public:
  GQLAssignmentItem(String name_, Ptr value_, bool value_keyword_ = false, Ptr type_ = {})
      : name(std::move(name_)), value(std::move(value_)), type(std::move(type_)), value_keyword(value_keyword_) {
    if (type) children.push_back(type);
    if (value) children.push_back(value);
  }

  String getID(char delim) const override { return "GQLAssignmentItem" + (delim + name); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLAssignmentItem>(*this);
    result->children.clear();
    result->type = type ? type->clone() : Ptr{};
    result->value = value ? value->clone() : Ptr{};

    if (result->type) result->children.push_back(result->type);
    if (result->value) result->children.push_back(result->value);

    return result;
  }

  String name;
  Ptr value;
  Ptr type;
  bool value_keyword = false;

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override {
    if (value_keyword) ostr << "VALUE ";

    ostr << name;

    if (type) {
      ostr << " ";
      type->format(ostr, settings, state, frame);
    }

    if (value) {
      ostr << " = ";
      value->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &type);
    f(nullptr, &value);
  }
};

}  // namespace DB::OPENGQL::AST
