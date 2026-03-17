#pragma once

#include <DataTypes/DataTypeLowCardinality.h>
#include <DataTypes/DataTypeNullable.h>
#include <DataTypes/DataTypeString.h>
#include <Functions/CastOverloadResolver.h>
#include <Functions/FunctionsLogical.h>
#include <Functions/IFunctionAdaptors.h>
#include <Interpreters/ExpressionActions.h>
#include <Storages/MergeTree/MergeTreeRangeReader.h>
#include <Storages/SelectQueryInfo.h>

namespace DB {

bool tryBuildPrewhereSteps(PrewhereInfoPtr prewhere_info, const ExpressionActionsSettings& actions_settings, PrewhereExprInfo& prewhere,
                           bool force_short_circuit_execution);

}
