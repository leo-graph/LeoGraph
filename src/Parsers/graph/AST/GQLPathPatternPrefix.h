#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPathPatternPrefix final : public DB::IAST {
 public:
  GQLPathPatternPrefix() = default;

  String getID(char) const override { return "GQLPathPatternPrefix"; }

  ASTPtr clone() const override { return make_intrusive<GQLPathPatternPrefix>(*this); }

  PathMode path_mode = PathMode::None;
  PathSearchKind search_kind = PathSearchKind::None;
  String count;
  bool has_path_keyword = false;
  bool use_paths_keyword = false;
  bool use_groups_keyword = false;

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

  void formatPathModeIfNeeded(WriteBuffer &ostr) const {
    if (path_mode == PathMode::None) return;

    ostr << getPathModeKeyword(path_mode);
  }

  void formatPathKeywordIfNeeded(WriteBuffer &ostr) const {
    if (!has_path_keyword) return;

    ostr << " " << getPathKeyword(use_paths_keyword);
  }

  void formatImpl(WriteBuffer &ostr, const FormatSettings &, FormatState &, FormatStateStacked) const override {
    switch (search_kind) {
      case PathSearchKind::None:
        formatPathModeIfNeeded(ostr);
        formatPathKeywordIfNeeded(ostr);
        return;

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
        ostr << "SHORTEST " << count;
        break;

      case PathSearchKind::CountedShortestGroup:
        ostr << "SHORTEST";
        if (!count.empty()) ostr << " " << count;
        break;
    }

    if (path_mode != PathMode::None) ostr << " " << getPathModeKeyword(path_mode);

    if (search_kind == PathSearchKind::CountedShortestGroup) {
      if (has_path_keyword) ostr << " " << getPathKeyword(use_paths_keyword);

      ostr << " " << (use_groups_keyword ? "GROUPS" : "GROUP");
      return;
    }

    if (has_path_keyword) ostr << " " << getPathKeyword(use_paths_keyword);
  }
};

}  // namespace DB::OPENGQL::AST
