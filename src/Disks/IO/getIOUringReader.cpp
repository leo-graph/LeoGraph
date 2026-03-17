#include <Disks/IO/getIOUringReader.h>

#if USE_LIBURING

#  include <Common/ErrorCodes.h>
#  include <Interpreters/Context.h>

namespace DB {

namespace ErrorCodes {
extern const int LOGICAL_ERROR;
extern const int UNSUPPORTED_METHOD;
}  // namespace ErrorCodes

std::unique_ptr<IOUringReader> createIOUringReader() { return std::make_unique<IOUringReader>(512); }

IOUringReader& getIOUringReaderOrThrow(ContextPtr context) {
  auto& reader = context->getIOUringReader();
  if (!reader.isSupported()) {
    throw Exception(ErrorCodes::UNSUPPORTED_METHOD, "io_uring is not supported by this system");
  }
  return reader;
}

IOUringReader& getIOUringReaderOrThrow() {
  auto context = Context::getGlobalContextInstance();
  if (!context) throw Exception(ErrorCodes::LOGICAL_ERROR, "Global context not initialized");
  return getIOUringReaderOrThrow(context);
}

}  // namespace DB
#endif
