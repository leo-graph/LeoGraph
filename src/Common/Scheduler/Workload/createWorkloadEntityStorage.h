#pragma once

#include <Common/Scheduler/Workload/IWorkloadEntityStorage.h>
#include <Interpreters/Context_fwd.h>

namespace DB {

std::unique_ptr<IWorkloadEntityStorage> createWorkloadEntityStorage(const ContextMutablePtr& global_context);

}
