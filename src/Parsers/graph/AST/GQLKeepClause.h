#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLKeepClause final : public DB::IAST {
 public:
  Ptr path_prefix;

  String getID(char) const override { return "GQLKeepClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLKeepClause>(*this);
    result->children.clear();
    result->path_prefix = path_prefix ? path_prefix->clone() : Ptr{};
    if (result->path_prefix) result->children.push_back(result->path_prefix);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    chassert(path_prefix);
    ostr << "KEEP ";
    path_prefix->format(ostr, settings, state, frame);
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &path_prefix); }
};

}  // namespace DB::OPENGQL::AST
