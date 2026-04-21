#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLPathSearchPrefix final : public DB::IAST {
 public:
  PathSearchKind search_kind = PathSearchKind::None;
  CountKind count_kind = CountKind::None;
  Ptr count;
  PathMode path_mode = PathMode::None;
  bool has_path_keyword = false;
  bool use_paths_keyword = false;
  bool use_groups_keyword = false;

  String getID(char) const override { return "GQLPathSearchPrefix"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLPathSearchPrefix>(*this);
    result->children.clear();
    result->count = count ? count->clone() : Ptr{};
    if (result->count) result->children.push_back(result->count);
    return result;
  }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    chassert(search_kind != PathSearchKind::None);

    switch (search_kind) {
      case PathSearchKind::All:
        ostr << "ALL";
        break;
      case PathSearchKind::Any:
        ostr << "ANY";
        if (count) {
          ostr << " ";
          count->format(ostr, settings, state, frame);
        }
        break;
      case PathSearchKind::AllShortest:
        ostr << "ALL SHORTEST";
        break;
      case PathSearchKind::AnyShortest:
        ostr << "ANY SHORTEST";
        break;
      case PathSearchKind::CountedShortest:
        chassert(count);
        ostr << "SHORTEST ";
        count->format(ostr, settings, state, frame);
        break;
      case PathSearchKind::CountedShortestGroup:
        ostr << "SHORTEST";
        if (count) {
          ostr << " ";
          count->format(ostr, settings, state, frame);
        }
        break;
      case PathSearchKind::None:
        return;
    }

    if (path_mode != PathMode::None) ostr << " " << detail::getPathModeKeyword(path_mode);
    if (has_path_keyword) ostr << " " << detail::getPathKeyword(use_paths_keyword);

    if (search_kind == PathSearchKind::CountedShortestGroup) {
      chassert(count_kind == CountKind::Groups);
      ostr << " " << (use_groups_keyword ? "GROUPS" : "GROUP");
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override { f(nullptr, &count); }
};

}  // namespace DB::OPENGQL::AST
