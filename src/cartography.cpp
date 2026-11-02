// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "cartography.h"
#include <cmath>
#include <algorithm>
#include <array>
#include <map>
#include <cctype>
#include "../third_party/earcut/earcut.hpp"
namespace dx {
namespace {constexpr double pi=3.14159265358979323846;}
Point mercator(Point p){return {(1-std::asinh(std::tan(std::clamp(p.lat,-85.05112878,85.05112878)*pi/180))/pi)/2,(p.lon+180)/360};}
Point geographic(Point p){return {std::atan(std::sinh(pi*(1-2*p.lat)))*180/pi,p.lon*360-180};}
bool insidePolygon(Point p,const std::vector<std::vector<Point>>& rings){
 if(!std::isfinite(p.lat)||!std::isfinite(p.lon)||std::abs(p.lat)>90)return false;
 auto longitude=[](double previous,Point a,Point b){
  const double delta=b.lon-a.lon;
  // Preserve the full-width polar closure in the Antarctica outline.
  if(std::abs(delta)==360&&std::abs(a.lat)==90&&a.lat==b.lat)return previous+delta;
  return previous+std::remainder(delta,360.);
 };
 auto inside=[&](const std::vector<Point>& ring){
  if(ring.size()<3)return false;
  // Keep the ring continuous, then place the pointer in that same world copy.
  // Wrapping each vertex around the pointer opens a seam on the opposite side
  // of Earth and can make an unrelated country pass the ray-crossing test.
  double x=ring.front().lon,left=x,right=x,bottom=ring.front().lat,top=bottom;
  for(size_t i=1;i<ring.size();++i){x=longitude(x,ring[i-1],ring[i]);left=std::min(left,x);right=std::max(right,x);bottom=std::min(bottom,ring[i].lat);top=std::max(top,ring[i].lat);}
  const double middle=(left+right)/2,px=middle+std::remainder(p.lon-middle,360.);
  if(px<left||px>right||p.lat<bottom||p.lat>top)return false;
  bool hit=false;x=ring.front().lon;
  for(size_t i=1;i<=ring.size();++i){const auto a=ring[i-1],b=ring[i%ring.size()];const double z=longitude(x,a,b);
   if((a.lat>p.lat)!=(b.lat>p.lat)&&px<(z-x)*(p.lat-a.lat)/(b.lat-a.lat)+x)hit=!hit;
   x=z;
  }
  return hit;
 };
 if(rings.empty()||!inside(rings[0]))return false;for(size_t i=1;i<rings.size();i++)if(inside(rings[i]))return false;return true;
}
Point countryCenter(const std::vector<Point>& ring,Point fallback){
 if(ring.size()<3)return fallback;double area=0,x=0,y=0,base=ring[0].lon;
 for(size_t i=0,j=ring.size()-1;i<ring.size();j=i++){
  double a=base+std::remainder(ring[j].lon-base,360.),b=base+std::remainder(ring[i].lon-base,360.);double cross=a*ring[i].lat-b*ring[j].lat;
  area+=cross;x+=(a+b)*cross;y+=(ring[j].lat+ring[i].lat)*cross;
 }
 if(std::abs(area)<1e-9)return fallback;Point center{y/(3*area),std::remainder(x/(3*area),360.)};
 return insidePolygon(center,{ring})?center:fallback;
}
bool labelFits(Point center,double width,double height,const std::vector<std::vector<Point>>& rings){
 if(width<=0||height<=0||width>=180||height>=180)return false;
 // Sample the full box perimeter, including narrow borders and polygon holes.
 for(int i=0;i<=8;i++){double t=i/8.;for(Point p:std::vector<Point>{{center.lat-height/2,center.lon-width/2+t*width},{center.lat+height/2,center.lon-width/2+t*width},{center.lat-height/2+t*height,center.lon-width/2},{center.lat-height/2+t*height,center.lon+width/2}})if(!insidePolygon(p,rings))return false;}
 for(const auto& ring:rings)for(auto p:ring)if(std::abs(std::remainder(p.lon-center.lon,360.))<width/2&&std::abs(p.lat-center.lat)<height/2)return false;
 return insidePolygon(center,rings);
}
CountryMesh countryMesh(const std::vector<std::vector<Point>>& rings,bool online){
 CountryMesh mesh;if(rings.empty()||rings[0].empty())return mesh;
 std::vector<std::vector<std::array<double,2>>> shape;double base=rings[0][0].lon;
 for(const auto& ring:rings){if(ring.size()<3)continue;std::vector<std::array<double,2>> points;
  for(auto point:ring){point.lon=base+std::remainder(point.lon-base,360.);if(online)point.lat=mercator(point).lat*360.;points.push_back({point.lon,point.lat});mesh.points.push_back(point);mesh.min.lat=std::min(mesh.min.lat,point.lat);mesh.min.lon=std::min(mesh.min.lon,point.lon);mesh.max.lat=std::max(mesh.max.lat,point.lat);mesh.max.lon=std::max(mesh.max.lon,point.lon);}shape.push_back(std::move(points));
 }
 mesh.indices=mapbox::earcut<uint32_t>(shape);return mesh;
}
bool northAmericanLocation(Point home,const Json& world){
 if(!std::isfinite(home.lat)||!std::isfinite(home.lon)||!world.is_object()||!world.contains("features")||!world["features"].is_array())return false;
 for(const auto& feature:world["features"]){
  if(feature["properties"].value("CONTINENT","")!="North America")continue;
  auto coordinates=feature["geometry"]["coordinates"];if(feature["geometry"]["type"]=="Polygon")coordinates=Json::array({coordinates});
  for(const auto& polygon:coordinates){std::vector<std::vector<Point>> rings;for(const auto& ring:polygon){std::vector<Point> points;for(const auto& p:ring)points.push_back({p[1].get<double>(),p[0].get<double>()});rings.push_back(std::move(points));}if(insidePolygon(home,rings))return true;}
 }return false;
}
std::string countryOutlineKey(const std::string& country){
 std::string key;for(unsigned char c:upper(country))if(std::isalnum(c))key+=char(c);
 static const std::map<std::string,std::string> aliases={
  {"FEDREPOFGERMANY","GERMANY"},{"EUROPEANRUSSIA","RUSSIA"},{"ASIATICRUSSIA","RUSSIA"},{"KALININGRAD","RUSSIA"},
  {"ASIATICTURKEY","TURKEY"},{"EUROPEANTURKEY","TURKEY"},{"BOSNIAHERZEGOVINA","BOSNIAANDHERZEGOVINA"},
  {"ENGLAND","UNITEDKINGDOM"},{"SCOTLAND","UNITEDKINGDOM"},{"WALES","UNITEDKINGDOM"},{"NORTHERNIRELAND","UNITEDKINGDOM"},
  {"SARDINIA","ITALY"},{"SICILY","ITALY"},{"CRETE","GREECE"},{"DODECANESE","GREECE"},
  {"CANARYISLANDS","SPAIN"},{"MADEIRAISLANDS","PORTUGAL"}
 };auto found=aliases.find(key);return found==aliases.end()?key:found->second;
}
}
