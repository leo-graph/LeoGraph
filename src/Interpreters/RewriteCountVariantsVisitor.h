#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

namespace DB {

class ASTFunction;

class RewriteCountVariantsVisitor {
 public:
  explicit RewriteCountVariantsVisitor(ContextPtr context_) : context(context_) {}
  void visit(ASTPtr &);
  void visit(ASTFunction &);

 private:
  ContextPtr context;
};

}  // namespace DB
