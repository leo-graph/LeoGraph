#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLMatchClause final : public DB::IAST {
 public:
  explicit GQLMatchClause(bool optional_ = false) : optional(optional_) {}

  String getID(char) const override { return optional ? "GQLOptionalMatchClause" : "GQLMatchClause"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLMatchClause>(*this);
    result->children.clear();
    detail::cloneChildrenList(path_patterns, result->path_patterns, result->children);
    result->where = where ? where->clone() : Ptr{};

    if (result->where) result->children.push_back(result->where);

    return result;
  }

  bool optional;
  PtrList path_patterns;
  Ptr where;

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << (optional ? "OPTIONAL MATCH " : "MATCH ");
    detail::formatChildren(ostr, settings, state, frame, path_patterns, ", ");

    if (where) {
      ostr << " ";
      where->format(ostr, settings, state, frame);
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    for (auto &path : path_patterns) f(nullptr, &path);

    f(nullptr, &where);
  }
};

}  // namespace DB::OPENGQL::AST
