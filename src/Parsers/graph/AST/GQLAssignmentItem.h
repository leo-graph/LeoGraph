#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLAssignmentItem final : public DB::IAST {
 public:
  GQLAssignmentItem(String name_, Ptr value_, bool value_keyword_ = false, String raw_type_ = {})
      : name(std::move(name_)), value(std::move(value_)), value_keyword(value_keyword_), raw_type(std::move(raw_type_)) {
    if (value) children.push_back(value);
  }

  String getID(char delim) const override { return "GQLAssignmentItem" + (delim + name); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLAssignmentItem>(*this);
    result->children.clear();
    result->value = value ? value->clone() : Ptr{};

    if (result->value) result->children.push_back(result->value);

    return result;
  }

  String name;
  Ptr value;
  bool value_keyword = false;
  String raw_type;

 protected:
  void formatImpl(WriteBuffer & ostr, const FormatSettings & settings, FormatState & state, FormatStateStacked frame) const override {
    if (value_keyword) ostr << "VALUE ";

    ostr << name;

    if (!raw_type.empty()) ostr << " " << raw_type;

    if (value) {
      ostr << " = ";
      value->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &value); }
};

}  // namespace DB::OPENGQL::AST
