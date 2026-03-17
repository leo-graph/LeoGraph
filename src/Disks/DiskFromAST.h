#pragma once
#include <Interpreters/Context_fwd.h>
#include <Parsers/IAST_fwd.h>
#include <string>

namespace DB {

namespace DiskFromAST

{
void ensureDiskIsNotCustom(const std::string& name, ContextPtr context);
std::string createCustomDisk(const ASTPtr& disk_function, ContextPtr context, bool attach);
}  // namespace DiskFromAST

}  // namespace DB
