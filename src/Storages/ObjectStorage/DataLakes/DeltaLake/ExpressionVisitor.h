#pragma once

#include "config.h"

#if USE_DELTA_KERNEL_RS

#  include <Core/Field.h>
#  include <Interpreters/ActionsDAG.h>

namespace ffi {
struct Expression;
struct SharedExpression;
struct SharedPredicate;
}  // namespace ffi
namespace DB {
class Chunk;
}

namespace DeltaLake {

/// Get values for the `columns` considering that
/// they contain literal (constant) values.
/// This is used, for example, to get partition values.
std::vector<DB::Field> getConstValuesFromExpression(const DB::Names& columns, const DB::ActionsDAG& dag);

/// Visit exception for scanCallback.
std::shared_ptr<DB::ActionsDAG> visitScanCallbackExpression(ffi::SharedExpression* expression, const DB::NamesAndTypesList& read_schema,
                                                            const DB::NamesAndTypesList& expression_schema, bool enable_logging);

/// A method used in unit test.
std::shared_ptr<DB::ActionsDAG> visitExpression(ffi::SharedPredicate* expression, const DB::NamesAndTypesList& read_schema,
                                                const DB::NamesAndTypesList& expression_schema);

}  // namespace DeltaLake

#endif
