#include <Common/Exception.h>
#include <IO/VarInt.h>

namespace DB {
namespace ErrorCodes {
extern const int ATTEMPT_TO_READ_AFTER_EOF;
}

void throwReadAfterEOF() { throw Exception(ErrorCodes::ATTEMPT_TO_READ_AFTER_EOF, "Attempt to read after eof"); }

}  // namespace DB
