#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPropertyItem final : public DB::IAST {
 public:
  GQLPropertyItem(String key_, Ptr value_) : key(std::move(key_)), value(std::move(value_)) {
    if (value) children.push_back(value);
  }

  String getID(char delim) const override { return "GQLPropertyItem" + (delim + key); }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPropertyItem>(*this);
    result->children.clear();
    result->value.reset();

    if (value) {
      result->value = value->clone();
      result->children.push_back(result->value);
    }

    return result;
  }

  String key;
  Ptr value;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << key << ": ";

    if (value) value->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &value); }
};

}  // namespace DB::OPENGQL::AST
