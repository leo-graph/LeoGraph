#pragma once

#include <list>

#include <parser/AST/CypherCallProcedureClause.h>
#include <parser/AST/CypherDeleteFromClause.h>
#include <parser/AST/CypherInsertIntoClause.h>
#include <parser/AST/CypherPattern.h>
#include <parser/AST/CypherProjectClause.h>
#include <parser/AST/CypherUnwindClause.h>
#include <parser/AST/CypherUpdateIntoClause.h>
#include <parser/AST/CypherWhereClause.h>
#include <parser/AST/INode.h>

namespace DB::AST {

class CypherClausesQuery : public INodeHelper<CypherClausesQuery> {
 public:
  explicit CypherClausesQuery(PtrList&& list) : INodeHelper(std::move(list)) {}

  [[nodiscard]] size_t clauseSize() const { return size(); }
  [[nodiscard]] Ptr getClause(unsigned idx) const { return get(idx); }

  void formatImpl(WriteBuffer& os, const FormatSettings& format_settings, FormatStateStacked frame) const override;
  [[nodiscard]] AstType getAstType() const override { return AstType::CypherClausesQuery; }
  [[nodiscard]] bool equal(const INode& other) const override {
    if (auto* o = typeid_cast<decltype(this)>(&other)) {
      return childrenEqual(*o);
    }
    return false;
  }
  void updateHash(UUID& hash) const override {
    updateNameHash(hash);
    updateChildrenHash(hash);
  }
};
}  // namespace DB::AST
