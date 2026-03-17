#pragma once

#include <base/types.h>
#include <Core/NamesAndTypes.h>
#include <Parsers/IAST_fwd.h>

namespace DB {

/// Replace subcolumns to getSubcolumn() function.
void replaceSubcolumnsToGetSubcolumnFunctionInQuery(ASTPtr& ast, const NamesAndTypesList& columns);

}  // namespace DB
