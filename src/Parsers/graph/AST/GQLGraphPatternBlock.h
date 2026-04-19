#pragma once

#include <Parsers/graph/AST/Utils.h>

namespace DB::OPENGQL::AST {

class GQLGraphPatternBlock final : public DB::IAST {
 public:
  bool parenthesized = false;
  GraphMatchMode match_mode = GraphMatchMode::None;
  Ptr keep_clause;
  PtrList path_patterns;
  Ptr where;

  String getID(char) const override { return "GQLGraphPatternBlock"; }

  ASTPtr clone() const override {
    auto result = make_intrusive<GQLGraphPatternBlock>(*this);
    result->children.clear();
    result->keep_clause = keep_clause ? keep_clause->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};

    detail::cloneChildrenList(path_patterns, result->path_patterns, result->children);
    if (result->keep_clause) result->children.push_back(result->keep_clause);
    if (result->where) result->children.push_back(result->where);

    return result;
  }

 protected:
  static void formatMatchMode(WriteBuffer &ostr, GraphMatchMode mode) {
    switch (mode) {
      case GraphMatchMode::RepeatableElements:
        ostr << "REPEATABLE ELEMENTS";
        return;
      case GraphMatchMode::RepeatableElementBindings:
        ostr << "REPEATABLE ELEMENT BINDINGS";
        return;
      case GraphMatchMode::DifferentEdges:
        ostr << "DIFFERENT EDGES";
        return;
      case GraphMatchMode::DifferentEdgeBindings:
        ostr << "DIFFERENT EDGE BINDINGS";
        return;
      case GraphMatchMode::None:
        return;
    }
  }

  void formatImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &state, FormatStateStacked frame) const override {
    ostr << (parenthesized ? "(" : "{");

    bool has_content = false;

    auto write_space = [&]() {
      ostr << (has_content ? " " : " ");
      has_content = true;
    };

    if (match_mode != GraphMatchMode::None) {
      write_space();
      formatMatchMode(ostr, match_mode);
    }

    if (!path_patterns.empty()) {
      write_space();
      detail::formatChildren(ostr, settings, state, frame, path_patterns, ", ");
    }

    if (keep_clause) {
      write_space();
      keep_clause->format(ostr, settings, state, frame);
    }

    if (where) {
      write_space();
      where->format(ostr, settings, state, frame);
    }

    if (has_content) ostr << " ";

    ostr << (parenthesized ? ")" : "}");
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &keep_clause);

    for (auto &path : path_patterns) f(nullptr, &path);

    f(nullptr, &where);
  }
};

}  // namespace DB::OPENGQL::AST
