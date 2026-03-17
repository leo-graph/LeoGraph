#pragma once
#include <Core/Types.h>
#include <Parsers/IAST_fwd.h>

namespace DB {
ASTPtr tryGetTableOverride(const String& mapped_database, const String& table);
}
