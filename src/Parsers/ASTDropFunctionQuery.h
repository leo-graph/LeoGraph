#pragma once

#include <Parsers/ASTQueryWithOnCluster.h>
#include <Parsers/IAST.h>

namespace DB {

class ASTDropFunctionQuery : public IAST, public ASTQueryWithOnCluster {
 public:
  String function_name;

  bool if_exists = false;

  String getID(char) const override { return "DropFunctionQuery"; }

  ASTPtr clone() const override;

  ASTPtr getRewrittenASTWithoutOnCluster(const WithoutOnClusterASTRewriteParams &) const override {
    return removeOnCluster<ASTDropFunctionQuery>(clone());
  }

  QueryKind getQueryKind() const override { return QueryKind::Drop; }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &s, FormatState &state, FormatStateStacked frame) const override;
};

}  // namespace DB
