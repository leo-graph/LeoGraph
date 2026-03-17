#pragma once

#include <Analyzer/IQueryTreePass.h>
#include <base/types.h>
#include <Columns/IColumn.h>

void testPassOnCondition(DB::QueryTreePassPtr pass, DB::DataTypePtr columnType, const String& cond, const String& expected);
