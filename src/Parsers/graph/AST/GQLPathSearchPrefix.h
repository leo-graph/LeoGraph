#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPathSearchPrefix final : public DB::IAST {
 public:
  PathSearchKind search_kind = PathSearchKind::None;
  CountKind count_kind = CountKind::None;
  String count;
  PathMode path_mode = PathMode::None;
  bool has_path_keyword = false;
  bool use_paths_keyword = false;
  bool use_groups_keyword = false;

  String getID(char) const override { return "GQLPathSearchPrefix"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPathSearchPrefix>(*this);
    result->children.clear();
    return result;
  }

 protected:
  static const char *getPathModeKeyword(PathMode mode) {
    switch (mode) {
      case PathMode::Walk:
        return "WALK";
      case PathMode::Trail:
        return "TRAIL";
      case PathMode::Simple:
        return "SIMPLE";
      case PathMode::Acyclic:
        return "ACYCLIC";
      case PathMode::None:
        return "";
    }

    return "";
  }

  static const char *getPathKeyword(bool use_paths_keyword) { return use_paths_keyword ? "PATHS" : "PATH"; }

  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override {
    chassert(search_kind != PathSearchKind::None);

    switch (search_kind) {
      case PathSearchKind::All:
        ostr << "ALL";
        break;
      case PathSearchKind::Any:
        ostr << "ANY";
        if (!count.empty()) ostr << " " << count;
        break;
      case PathSearchKind::AllShortest:
        ostr << "ALL SHORTEST";
        break;
      case PathSearchKind::AnyShortest:
        ostr << "ANY SHORTEST";
        break;
      case PathSearchKind::CountedShortest:
        chassert(!count.empty());
        ostr << "SHORTEST " << count;
        break;
      case PathSearchKind::CountedShortestGroup:
        ostr << "SHORTEST";
        if (!count.empty()) ostr << " " << count;
        break;
      case PathSearchKind::None:
        return;
    }

    if (path_mode != PathMode::None) ostr << " " << getPathModeKeyword(path_mode);
    if (has_path_keyword) ostr << " " << getPathKeyword(use_paths_keyword);

    if (search_kind == PathSearchKind::CountedShortestGroup) {
      chassert(count_kind == CountKind::Groups);
      ostr << " " << (use_groups_keyword ? "GROUPS" : "GROUP");
    }
  }
};

}  // namespace DB::OPENGQL::AST
