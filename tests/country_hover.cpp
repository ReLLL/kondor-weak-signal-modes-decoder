// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "cartography.h"
#include <fstream>
#include <iostream>
#include <utility>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::ifstream input(argv[1]);dx::Json world;input>>world;
 using Polygon=std::vector<std::vector<dx::Point>>;
 std::vector<std::pair<std::string,std::vector<Polygon>>> countries;
 for(const auto& feature:world["features"]){
  auto coordinates=feature["geometry"]["coordinates"];
  if(feature["geometry"]["type"]=="Polygon")coordinates=dx::Json::array({coordinates});
  std::vector<Polygon> polygons;
  for(const auto& polygon:coordinates){Polygon rings;for(const auto& ring:polygon){std::vector<dx::Point> points;for(const auto& p:ring)points.push_back({p[1],p[0]});rings.push_back(std::move(points));}polygons.push_back(std::move(rings));}
  countries.emplace_back(feature["properties"].value("NAME_EN",feature["properties"].value("NAME","")),std::move(polygons));
 }
 // Same first-containing-country lookup as the hover path, in source order.
 auto hover=[&](dx::Point point){for(const auto& [name,polygons]:countries)for(const auto& polygon:polygons)if(dx::insidePolygon(point,polygon))return name;return std::string{};};
 const std::vector<std::pair<dx::Point,std::string>> samples={
  {{22,79},"India"},{{60,105},"Russia"},{{35,105},"People's Republic of China"},
  {{37,138},"Japan"},{{-25,134},"Australia"},{{55,-105},"Canada"},
  {{40,-100},"United States of America"},{{23,-102},"Mexico"},{{40,160},""},
  {{-85,0},"Antarctica"},{{-85,150},"Antarctica"},{{-87,-150},"Antarctica"},{{-85,-150},""},{{0,0},""}
 };
 for(const auto& [point,expected]:samples)for(int copy=-2;copy<=2;++copy){
  auto actual=hover({point.lat,point.lon+360*copy});
  if(actual!=expected){std::cerr<<point.lat<<", "<<point.lon<<": expected "<<expected<<", got "<<actual<<'\n';return 1;}
 }
 Polygon seam={{{-2,179},{-2,-179},{2,-179},{2,179},{-2,179}},{{-1,179.5},{-1,-179.5},{1,-179.5},{1,179.5},{-1,179.5}}};
 CHECK(dx::insidePolygon({1.5,179.8},seam));CHECK(dx::insidePolygon({1.5,-179.8},seam));
 CHECK(!dx::insidePolygon({0,179.8},seam));CHECK(!dx::insidePolygon({0,0},seam));
 CHECK(!dx::insidePolygon({NAN,0},seam));CHECK(!dx::insidePolygon({0,NAN},seam));
 CHECK(!dx::insidePolygon({0,0},{}));CHECK(!dx::insidePolygon({0,0},{{{0,0},{1,1}}}));
 for(dx::Point home:std::vector<dx::Point>{{39,-98},{45,-75},{19,-99},{9,-80},{18,-77},{61,-150}})CHECK(dx::northAmericanLocation(home,world));
 for(dx::Point home:std::vector<dx::Point>{{39,33},{48,2},{6,-58},{-12,-76},{0,0},{NAN,0}})CHECK(!dx::northAmericanLocation(home,world));
 CHECK(!dx::northAmericanLocation({39,-98},dx::Json{}));
 std::cout<<"Country hover: eastern/western countries, ocean, world copies, dateline holes and Antarctica passed\n";
}
