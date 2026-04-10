#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLRecordConstructor final : public DB::IAST {
 public:
  bool explicit_record_keyword = false;
  PtrList fields;

  String getID(char) const override { return "GQLRecordConstructor"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLRecordConstructor>(*this);
    result->children.clear();
    detail::cloneChildrenList(fields, result->fields, result->children);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    if (explicit_record_keyword) ostr << "RECORD ";

    ostr << "{";
    detail::formatChildren(ostr, settings, state, frame, fields, ", ");
    ostr << "}";
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &field : fields) f(nullptr, &field);
  }
};

}  // namespace DB::OPENGQL::AST
