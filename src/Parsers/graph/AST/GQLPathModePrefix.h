#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPathModePrefix final : public DB::IAST {
 public:
  PathMode path_mode = PathMode::None;
  bool has_path_keyword = false;
  bool use_paths_keyword = false;

  String getID(char) const override { return "GQLPathModePrefix"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPathModePrefix>(*this);
    result->children.clear();
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override {
    chassert(path_mode != PathMode::None);
    ostr << detail::getPathModeKeyword(path_mode);

    if (has_path_keyword) ostr << " " << detail::getPathKeyword(use_paths_keyword);
  }
};

}  // namespace DB::OPENGQL::AST
