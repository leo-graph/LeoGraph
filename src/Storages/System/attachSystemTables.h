#pragma once

#include <Interpreters/Context_fwd.h>
#include <memory>

namespace DB {

class AsynchronousMetrics;
class IDatabase;

void attachSystemTablesServer(ContextPtr context, IDatabase& system_database, bool has_zookeeper);
void attachSystemTablesAsync(ContextPtr context, IDatabase& system_database, AsynchronousMetrics& async_metrics);

}  // namespace DB
