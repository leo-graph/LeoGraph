#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPathPattern final : public DB::IAST {
 public:
  GQLPathPattern() = default;

  explicit GQLPathPattern(PtrList elements_) : elements(std::move(elements_)) {
    for (const auto &element : elements) {
      if (element) children.push_back(element);
    }
  }

  String getID(char) const override { return "GQLPathPattern"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPathPattern>(*this);
    result->children.clear();
    result->variable = variable ? variable->clone() : Ptr{};
    result->prefix = prefix ? prefix->clone() : Ptr{};

    if (result->variable) result->children.push_back(result->variable);
    if (result->prefix) result->children.push_back(result->prefix);

    detail::cloneChildrenList(elements, result->elements, result->children);
    return result;
  }

  Ptr variable;
  Ptr prefix;
  PtrList elements;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (variable) {
      variable->format(ostr, settings, state, frame);
      ostr << " = ";
    }

    if (prefix) {
      prefix->format(ostr, settings, state, frame);
      ostr << " ";
    }

    detail::formatChildren(ostr, settings, state, frame, elements, "");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &variable);
    f(nullptr, &prefix);

    for (auto &element : elements) f(nullptr, &element);
  }
};

}  // namespace DB::OPENGQL::AST
