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
    result->keep_clause = keep_clause ? keep_clause->clone() : Ptr{};
    result->where = where ? where->clone() : Ptr{};
    result->optional_operand_block = optional_operand_block ? optional_operand_block->clone() : Ptr{};

    if (result->keep_clause) result->children.push_back(result->keep_clause);
    detail::cloneChildrenList(path_patterns, result->path_patterns, result->children);
    detail::cloneChildrenList(yield_items, result->yield_items, result->children);

    if (result->where) result->children.push_back(result->where);
    if (result->optional_operand_block) result->children.push_back(result->optional_operand_block);

    return result;
  }

  bool optional;
  GraphMatchMode match_mode = GraphMatchMode::None;
  Ptr keep_clause;
  PtrList path_patterns;
  PtrList yield_items;
  Ptr where;
  Ptr optional_operand_block;

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
    if (optional_operand_block) {
      ostr << "OPTIONAL ";
      optional_operand_block->format(ostr, settings, state, frame);
      return;
    }

    ostr << (optional ? "OPTIONAL MATCH " : "MATCH ");

    if (match_mode != GraphMatchMode::None) {
      formatMatchMode(ostr, match_mode);
      ostr << " ";
    }

    detail::formatChildren(ostr, settings, state, frame, path_patterns, ", ");

    if (keep_clause) {
      ostr << " KEEP ";
      keep_clause->format(ostr, settings, state, frame);
    }

    if (where) {
      ostr << " ";
      where->format(ostr, settings, state, frame);
    }

    if (!yield_items.empty()) {
      ostr << " YIELD ";
      detail::formatChildren(ostr, settings, state, frame, yield_items, ", ");
    }
  }

  void forEachPointerToChild(std::function<void(IAST **, boost::intrusive_ptr<IAST> *)> f) override {
    f(nullptr, &keep_clause);
    for (auto &path : path_patterns) f(nullptr, &path);
    for (auto &item : yield_items) f(nullptr, &item);

    f(nullptr, &where);
    f(nullptr, &optional_operand_block);
  }
};

}  // namespace DB::OPENGQL::AST
