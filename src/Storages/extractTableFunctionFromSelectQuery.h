#pragma once

#include <Parsers/ASTExpressionList.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/IAST_fwd.h>

namespace DB {

ASTFunction* extractTableFunctionFromSelectQuery(ASTPtr& query);

}
