#pragma once

#include <Parsers/ASTQueryWithOnCluster.h>
#include <Parsers/ASTQueryWithTableAndOutput.h>

namespace DB {

/** UNDROP query
 */
class ASTUndropQuery : public ASTQueryWithTableAndOutput, public ASTQueryWithOnCluster {
 public:
  /** Get the text that identifies this element. */
  String getID(char) const override;
  ASTPtr clone() const override;

  ASTPtr getRewrittenASTWithoutOnCluster(const WithoutOnClusterASTRewriteParams &params) const override {
    return removeOnCluster<ASTUndropQuery>(clone(), params.default_database);
  }

  QueryKind getQueryKind() const override { return QueryKind::Undrop; }

 protected:
  void formatQueryImpl(WriteBuffer &ostr, const FormatSettings &settings, FormatState &, FormatStateStacked) const override;
};

}  // namespace DB
