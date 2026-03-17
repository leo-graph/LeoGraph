#pragma once

#include <Common/CurrentMetrics.h>
#include <Common/ProfileEvents.h>
#include <Common/ThreadPool_fwd.h>
#include <Core/NamesAndAliases.h>
#include <Core/NamesAndTypes.h>
#include <Interpreters/PeriodicLog.h>
#include <Storages/ColumnsDescription.h>

#include <ctime>
#include <vector>

namespace DB {

/** MetricLog is a log of metric values measured at regular time interval.
 */

struct MetricLogElement {
  time_t event_time{};
  Decimal64 event_time_microseconds{};

  std::vector<ProfileEvents::Count> profile_events;
  std::vector<CurrentMetrics::Metric> current_metrics;

  static std::string name() { return "MetricLog"; }
  static ColumnsDescription getColumnsDescription();
  static NamesAndAliases getNamesAndAliases() { return {}; }
  void appendToBlock(MutableColumns& columns) const;
};

class MetricLog : public PeriodicLog<MetricLogElement> {
  using PeriodicLog<MetricLogElement>::PeriodicLog;

 protected:
  void stepFunction(TimePoint current_time) override;

 private:
  /// stepFunction and flushBufferToLog may be executed concurrently, hence the mutex
  std::vector<ProfileEvents::Count> previous_profile_events TSA_GUARDED_BY(previous_profile_events_mutex) =
      std::vector<ProfileEvents::Count>(ProfileEvents::end());
  mutable std::mutex previous_profile_events_mutex;
};

}  // namespace DB
