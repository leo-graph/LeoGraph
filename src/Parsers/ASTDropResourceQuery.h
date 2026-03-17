#pragma once

#include <Parsers/ASTQueryWithOnCluster.h>
#include <Parsers/IAST.h>

namespace DB {

class ASTDropResourceQuery : public IAST, public ASTQueryWithOnCluster {
 public:
  String resource_name;

  bool if_exists = false;

  String getID(char) const override { return "DropResourceQuery"; }

  ASTPtr clone() const override;

  ASTPtr getRewrittenASTWithoutOnCluster(const WithoutOnClusterASTRewriteParams &) const override {
    return removeOnCluster<ASTDropResourceQuery>(clone());
  }

  QueryKind getQueryKind() const override { return QueryKind::Drop; }

 protected:
  void formatImpl(WriteBuffer &ostr, const FormatSettings &s, FormatState &state, FormatStateStacked frame) const override;
};

}  // namespace DB
