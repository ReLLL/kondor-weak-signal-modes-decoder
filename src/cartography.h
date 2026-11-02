// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace dx {
Point mercator(Point geographic);
Point geographic(Point mercator);
bool insidePolygon(Point point,const std::vector<std::vector<Point>>& rings);
bool northAmericanLocation(Point home,const Json& world);
Point countryCenter(const std::vector<Point>& outline,Point fallback);
bool labelFits(Point center,double width,double height,const std::vector<std::vector<Point>>& rings);
struct CountryMesh {std::vector<Point> points;std::vector<uint32_t> indices;Point min{1e9,1e9},max{-1e9,-1e9};};
CountryMesh countryMesh(const std::vector<std::vector<Point>>& rings,bool online);
std::string countryOutlineKey(const std::string& country);
}
