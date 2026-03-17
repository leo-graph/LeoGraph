#pragma once

#include <Common/IntervalKind.h>
#include <Parsers/IParser.h>

namespace DB {
/// Parses an interval kind.
bool parseIntervalKind(IParser::Pos& pos, Expected& expected, IntervalKind& result);
}  // namespace DB
