#include <Parsers/ASTFunction.h>
#include <Parsers/isDiskFunction.h>

namespace DB {

bool isDiskFunction(ASTPtr ast) {
  if (!ast) return false;

  const auto* function = ast->as<ASTFunction>();
  return function && function->name.starts_with("disk") && function->arguments->as<ASTExpressionList>();
}

}  // namespace DB
