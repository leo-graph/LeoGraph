#pragma once

#include <Common/CurrentThread.h>
#include <Poco/Message.h>
#include <utility>

std::pair<Poco::Message::Priority, DB::LogsLevel> parseSyslogLevel(int level);
