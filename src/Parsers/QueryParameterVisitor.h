#pragma once

#include <Core/Names.h>
#include <Parsers/IAST_fwd.h>
#include <string>

namespace DB {

/// Find parameters in a query and collect them into set.
NameSet analyzeReceiveQueryParams(const std::string& query);

NameSet analyzeReceiveQueryParams(const ASTPtr& ast);

NameToNameMap analyzeReceiveQueryParamsWithType(const ASTPtr& ast);

}  // namespace DB
