#pragma once

#include <Common/Scheduler/IResourceManager.h>
#include <Interpreters/Context_fwd.h>

namespace DB {

ResourceManagerPtr createResourceManager(const ContextMutablePtr& global_context);

}
