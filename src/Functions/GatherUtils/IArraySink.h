#pragma once

#include <Common/Exception.h>
#include <Functions/GatherUtils/ArraySinkVisitor.h>

namespace DB {

namespace ErrorCodes {
extern const int NOT_IMPLEMENTED;
}

namespace GatherUtils {

struct IArraySink {
  virtual ~IArraySink() = default;

  virtual void accept(ArraySinkVisitor &) {
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Accept not implemented for {}", demangle(typeid(*this).name()));
  }
};

template <typename Derived>
class ArraySinkImpl : public Visitable<Derived, IArraySink, ArraySinkVisitor> {};  /// NOLINT(bugprone-crtp-constructor-accessibility)

}  // namespace GatherUtils

}  // namespace DB
