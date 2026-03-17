#pragma once
#include <base/TypeLists.h>
#include <Common/Visitor.h>

namespace DB::GatherUtils {

template <typename T>
struct NumericArraySource;

struct GenericArraySource;

template <typename ArraySource>
struct NullableArraySource;

template <typename Base>
struct ConstSource;

using NumericArraySources = TypeListMap<NumericArraySource, TypeListNumberWithUUID>;
using BasicArraySources = TypeListAppend<GenericArraySource, NumericArraySources>;

class ArraySourceVisitor : public TypeListChangeRoot<Visitor, BasicArraySources> {
 protected:
  ~ArraySourceVisitor() = default;
};

template <typename Derived>
class ArraySourceVisitorImpl : public VisitorImpl<Derived, ArraySourceVisitor> {
 protected:
  ~ArraySourceVisitorImpl() = default;
};

}  // namespace DB::GatherUtils
