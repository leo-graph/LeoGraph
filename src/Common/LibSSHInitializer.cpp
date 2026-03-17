#include "config.h"

#include <Common/Exception.h>
#include <Common/LibSSHInitializer.h>

#if USE_SSH

#  include <Common/clibssh.h>

namespace DB {

namespace ErrorCodes {
extern const int SSH_EXCEPTION;
}

}  // namespace DB

namespace ssh {

LibSSHInitializer& LibSSHInitializer::instance() {
  static LibSSHInitializer instance;
  return instance;
}

LibSSHInitializer::LibSSHInitializer() {
  int rc = ssh_init();
  if (rc != SSH_OK) {
    throw DB::Exception(DB::ErrorCodes::SSH_EXCEPTION, "Failed to initialize libssh");
  }
}

LibSSHInitializer::~LibSSHInitializer() { ssh_finalize(); }

}  // namespace ssh

#else

namespace ssh {

LibSSHInitializer& LibSSHInitializer::instance() {
  static LibSSHInitializer instance;
  return instance;
}

LibSSHInitializer::LibSSHInitializer() {}
LibSSHInitializer::~LibSSHInitializer() {}

}  // namespace ssh

#endif
