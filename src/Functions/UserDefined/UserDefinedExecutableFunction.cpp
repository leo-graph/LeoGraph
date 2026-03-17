#include <Functions/UserDefined/UserDefinedExecutableFunction.h>

#include <Functions/FunctionFactory.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/IFunction.h>

#include <Processors/Sources/ShellCommandSource.h>

namespace DB {

UserDefinedExecutableFunction::UserDefinedExecutableFunction(const UserDefinedExecutableFunctionConfiguration& configuration_,
                                                             std::shared_ptr<ShellCommandSourceCoordinator> coordinator_,
                                                             const ExternalLoadableLifetime& lifetime_)
    : configuration(configuration_), coordinator(std::move(coordinator_)), lifetime(lifetime_) {}

}  // namespace DB
