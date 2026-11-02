// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <json.hpp>
#include <array>
#include <optional>
#include <vector>
namespace dx {
enum class ColumnMetric {Snr,Distance,Dial,Audio,Dt,Bearing,Count,MsgAge,MsgFirstAge,StationFirstAge,Confidence};
inline constexpr std::array<const char*,11> columnMetricKeys={"snr","distance","dial","audio","dt","bearing","count","msgAge","msgFirstAge","stationFirstAge","confidence"};
struct NumericRange {std::optional<double> min,max;bool active()const{return min.has_value()||max.has_value();}bool accepts(std::optional<double> value)const;};
using ColumnRanges=std::array<NumericRange,11>;
nlohmann::json columnRangesJson(const ColumnRanges& ranges);
ColumnRanges columnRangesFromJson(const nlohmann::json& value);
struct TableColumnSetting {int order=0;float width=0;bool visible=true;};
nlohmann::json columnSettingsJson(const std::vector<TableColumnSetting>& columns);
std::optional<std::vector<TableColumnSetting>> columnSettingsFromJson(const nlohmann::json& value,int count,bool appendColumn=false);
}
