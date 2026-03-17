#ifndef __clang_analyzer__  // It's too hard to analyze.

#  include <Functions/GatherUtils/Algorithms.h>
#  include <Functions/GatherUtils/GatherUtils.h>
#  include <Functions/GatherUtils/Selectors.h>

namespace DB::GatherUtils {

namespace {

struct SliceFromLeftConstantOffsetBoundedSelectArraySource
    : public ArraySourceSelector<SliceFromLeftConstantOffsetBoundedSelectArraySource> {
  template <typename Source>
  static void selectSource(bool is_const, bool is_nullable, Source &&source, size_t &offset, ssize_t &length,
                           ColumnArray::MutablePtr &result) {
    using SourceType = typename std::decay<Source>::type;
    using Sink = typename SourceType::SinkType;

    if (is_nullable) {
      using NullableSource = NullableArraySource<SourceType>;
      using NullableSink = typename NullableSource::SinkType;

      auto &nullable_source = static_cast<NullableSource &>(source);

      result = ColumnArray::create(nullable_source.createValuesColumn());
      NullableSink sink(result->getData(), result->getOffsets(), source.getColumnSize());

      if (is_const)
        sliceFromLeftConstantOffsetBounded(static_cast<ConstSource<NullableSource> &>(source), sink, offset, length);
      else
        sliceFromLeftConstantOffsetBounded(static_cast<NullableSource &>(source), sink, offset, length);
    } else {
      result = ColumnArray::create(source.createValuesColumn());
      Sink sink(result->getData(), result->getOffsets(), source.getColumnSize());

      if (is_const)
        sliceFromLeftConstantOffsetBounded(static_cast<ConstSource<SourceType> &>(source), sink, offset, length);
      else
        sliceFromLeftConstantOffsetBounded(source, sink, offset, length);
    }
  }
};

}  // namespace

ColumnArray::MutablePtr sliceFromLeftConstantOffsetBounded(IArraySource &src, size_t offset, ssize_t length) {
  ColumnArray::MutablePtr res;
  SliceFromLeftConstantOffsetBoundedSelectArraySource::select(src, offset, length, res);
  return res;
}
}  // namespace DB::GatherUtils

#endif
