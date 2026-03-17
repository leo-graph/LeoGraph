#include "config.h"

#if USE_ARROW || USE_PARQUET

#  include <arrow/chunked_array.h>
#  include <arrow/util/logging.h>
#  include <gtest/gtest.h>
#  include <vector>

namespace DB {

TEST(ChunkedArray, ChunkedArrayWithZeroChunksShouldNotAbort) {
  std::vector<std::shared_ptr<::arrow::Array>> empty_chunks_vector;

  EXPECT_ANY_THROW(::arrow::ChunkedArray{empty_chunks_vector});
}

TEST(ArrowLog, FatalLogShouldThrow) {
  EXPECT_ANY_THROW(::arrow::util::ArrowLog(__FILE__, __LINE__, ::arrow::util::ArrowLogLevel::ARROW_FATAL));
}

}  // namespace DB

#endif
