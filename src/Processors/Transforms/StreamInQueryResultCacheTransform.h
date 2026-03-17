#pragma once

#include <Interpreters/Cache/QueryResultCache.h>
#include <Processors/ISimpleTransform.h>

namespace DB {

class StreamInQueryResultCacheTransform : public ISimpleTransform {
 public:
  StreamInQueryResultCacheTransform(const Block& header_, std::shared_ptr<QueryResultCacheWriter> query_result_cache_writer,
                                    QueryResultCacheWriter::ChunkType chunk_type);

 protected:
  void transform(Chunk& chunk) override;

 public:
  void finalizeWriteInQueryResultCache();
  String getName() const override { return "StreamInQueryResultCacheTransform"; }

 private:
  const std::shared_ptr<QueryResultCacheWriter> query_result_cache_writer;
  const QueryResultCacheWriter::ChunkType chunk_type;
};

}  // namespace DB
