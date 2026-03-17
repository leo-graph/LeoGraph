#include <Common/CurrentThread.h>
#include <Core/Settings.h>
#include <Interpreters/Context.h>
#include <IO/WriteSettings.h>

namespace DB {

WriteSettings getWriteSettings() {
  auto query_context = CurrentThread::getQueryContext();
  if (query_context) return query_context->getWriteSettings();

  auto global_context = Context::getGlobalContextInstance();
  if (global_context) return global_context->getWriteSettings();

  return {};
}

WriteSettings getWriteSettingsForMetadata() {
  WriteSettings write_settings = getWriteSettings();
  write_settings.write_through_distributed_cache = false;

  return write_settings;
}
}  // namespace DB
