#pragma once

#include <Core/Names.h>
#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>

namespace DB {

/// Find parameters in a query parameter values and collect them into map.
NameToNameMap analyzeFunctionParamValues(const ASTPtr& ast, ContextPtr context);

}  // namespace DB
