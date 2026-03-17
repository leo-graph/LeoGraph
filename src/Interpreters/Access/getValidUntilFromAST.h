#pragma once

#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

#include <ctime>

namespace DB {
time_t getValidUntilFromAST(ASTPtr valid_until, ContextPtr context);
}
