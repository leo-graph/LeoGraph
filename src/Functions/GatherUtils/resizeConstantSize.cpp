#ifndef __clang_analyzer__  // It's too hard to analyze.

#  include <Functions/GatherUtils/Algorithms.h>
#  include <Functions/GatherUtils/GatherUtils.h>
#  include <Functions/GatherUtils/Selectors.h>

namespace DB::GatherUtils {

namespace {

struct ArrayResizeConstant : public ArrayAndValueSourceSelectorBySink<ArrayResizeConstant> {
  template <typename ArraySource, typename ValueSource, typename Sink>
  static void selectArrayAndValueSourceBySink(ArraySource&& array_source, ValueSource&& value_source, Sink&& sink, ssize_t size) {
    resizeConstantSize(array_source, value_source, sink, size);
  }
};

}  // namespace

void resizeConstantSize(IArraySource& array_source, IValueSource& value_source, IArraySink& sink, ssize_t size) {
  ArrayResizeConstant::select(sink, array_source, value_source, size);
}
}  // namespace DB::GatherUtils

#endif
