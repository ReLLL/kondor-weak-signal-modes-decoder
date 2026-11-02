// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "table_options.h"
#include <cmath>
namespace dx {
bool NumericRange::accepts(std::optional<double> value)const{if(!active())return true;return value&&std::isfinite(*value)&&(!min||*value>=*min)&&(!max||*value<=*max);}
nlohmann::json columnRangesJson(const ColumnRanges& ranges){auto out=nlohmann::json::object();for(size_t i=0;i<ranges.size();i++){const auto& r=ranges[i];if(!r.active())continue;auto value=nlohmann::json::object();if(r.min&&std::isfinite(*r.min))value["min"]=*r.min;if(r.max&&std::isfinite(*r.max))value["max"]=*r.max;if(!value.empty())out[columnMetricKeys[i]]=value;}return out;}
ColumnRanges columnRangesFromJson(const nlohmann::json& value){ColumnRanges out;if(!value.is_object())return out;for(size_t i=0;i<out.size();i++){auto field=value.find(columnMetricKeys[i]);if(field==value.end()||!field->is_object())continue;auto bound=[&](const char* key)->std::optional<double>{auto v=field->find(key);if(v==field->end()||!v->is_number())return {};double n=v->get<double>();return std::isfinite(n)?std::optional<double>{n}:std::nullopt;};out[i]={bound("min"),bound("max")};}return out;}
nlohmann::json columnSettingsJson(const std::vector<TableColumnSetting>& columns){auto out=nlohmann::json::array();for(const auto& c:columns)out.push_back({{"order",c.order},{"width",c.width},{"visible",c.visible}});return out;}
std::optional<std::vector<TableColumnSetting>> columnSettingsFromJson(const nlohmann::json& value,int count,bool appendColumn){
 if(appendColumn&&count>1&&value.is_array()&&value.size()==size_t(count-1)){auto old=columnSettingsFromJson(value,count-1);if(!old)return {};old->push_back({count-1,90,true});return old;}
 if(count<1||!value.is_array()||value.size()!=size_t(count))return {};std::vector<TableColumnSetting> out;std::vector<bool> seen(size_t(count),false);
 for(const auto& v:value){if(!v.is_object()||!v.contains("order")||!v["order"].is_number_integer()||!v.contains("width")||!v["width"].is_number()||!v.contains("visible")||!v["visible"].is_boolean())return {};auto order=v["order"].get<int64_t>();auto width=v["width"].get<double>();if(order<0||order>=count||seen[size_t(order)]||!std::isfinite(width)||width<0||width>10000)return {};seen[size_t(order)]=true;out.push_back({int(order),float(width),v["visible"].get<bool>()});}return out;
}
}
