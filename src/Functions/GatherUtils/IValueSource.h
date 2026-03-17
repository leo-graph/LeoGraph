#pragma once

#include <Common/Exception.h>
#include <Functions/GatherUtils/ValueSourceVisitor.h>

namespace DB {

namespace ErrorCodes {
extern const int NOT_IMPLEMENTED;
}

namespace GatherUtils {

struct IValueSource {
  virtual ~IValueSource() = default;

  virtual void accept(ValueSourceVisitor &) {
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Accept not implemented for {}", demangle(typeid(*this).name()));
  }

  virtual bool isConst() const { return false; }
};

template <typename Derived>
class ValueSourceImpl : public Visitable<Derived, IValueSource, ValueSourceVisitor> {};  /// NOLINT(bugprone-crtp-constructor-accessibility)

}  // namespace GatherUtils

}  // namespace DB
