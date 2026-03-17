#include <Storages/Kafka/IKafkaExceptionInfoSink.h>

#include <base/defines.h>
#include <cppkafka/error.h>

namespace DB {

void IKafkaExceptionInfoSink::setExceptionInfo(const cppkafka::Error& err, bool with_stacktrace) {
  setExceptionInfo(err.to_string(), with_stacktrace);
}

void IKafkaExceptionInfoSink::setExceptionInfo(const std::string& text, bool with_stacktrace) {
  UNUSED(text);
  UNUSED(with_stacktrace);
}

IKafkaExceptionInfoSink::~IKafkaExceptionInfoSink() = default;

}  // namespace DB
