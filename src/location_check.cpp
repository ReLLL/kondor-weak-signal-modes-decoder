// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "location_check.h"
#include <fstream>
#include <cmath>
#include <cctype>
namespace dx {
namespace {
constexpr double rad=3.141592653589793/180.,radius=6371.;
double edgeDistance(Point p,Point a,Point b){
 auto from=distanceBearing(a,p),edge=distanceBearing(a,b);if(edge.first<.001)return from.first;
 const double delta=from.first/radius,angle=(from.second-edge.second)*rad;
 const double along=std::atan2(std::sin(delta)*std::cos(angle),std::cos(delta))*radius;
 if(along<0||along>edge.first)return std::min(from.first,distanceBearing(b,p).first);
 return std::abs(std::asin(std::clamp(std::sin(delta)*std::sin(angle),-1.,1.)))*radius;
}
}
bool LocationCheck::load(const std::filesystem::path& file){
 areas_.clear();keys_.clear();cache_.clear();distanceKeys_.clear();distances_.clear();std::ifstream input(file);auto world=Json::parse(input,nullptr,false);if(world.is_discarded()||!world.contains("features"))return false;
 for(const auto& feature:world["features"]){Area area;const auto& geometry=feature["geometry"];auto coordinates=geometry["coordinates"];if(geometry["type"]=="Polygon")coordinates=Json::array({coordinates});else if(geometry["type"]!="MultiPolygon")continue;
  for(const auto& polygon:coordinates){std::vector<std::vector<Point>> rings;Point minimum{90,180},maximum{-90,-180};for(const auto& ring:polygon){std::vector<Point> points;for(const auto& p:ring){Point point{p[1].get<double>(),p[0].get<double>()};points.push_back(point);minimum.lat=std::min(minimum.lat,point.lat);minimum.lon=std::min(minimum.lon,point.lon);maximum.lat=std::max(maximum.lat,point.lat);maximum.lon=std::max(maximum.lon,point.lon);}rings.push_back(std::move(points));}area.polygons.push_back(std::move(rings));area.bounds.emplace_back(minimum,maximum);}
  if(area.polygons.empty())continue;const size_t index=areas_.size();areas_.push_back(std::move(area));const auto& props=feature["properties"];
  for(const char* name:{"NAME_EN","NAME","ADMIN","NAME_LONG","FORMAL_EN"})if(props.contains(name)&&props[name].is_string()){
   auto value=props[name].get<std::string>();keys_[countryOutlineKey(value)]=index;
   std::string exact;for(unsigned char c:upper(value))if(std::isalnum(c))exact+=char(c);distanceKeys_[exact]=index;
  }
 }
 if(auto found=keys_.find("UNITEDSTATESOFAMERICA");found!=keys_.end())keys_["UNITEDSTATES"]=found->second;
 // The short map label does not distinguish the two Congo countries.
 distanceKeys_.erase("CONGO");
 // Explicit aliases only: these apply to the optional country-distance view,
 // not DXCC identity or the country/grid mismatch filter. Regional labels use
 // the whole-country outline; unrelated islands/subdivisions remain unknown.
 for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
  {"UNITEDSTATES","UNITEDSTATESOFAMERICA"},{"FEDREPOFGERMANY","GERMANY"},
  {"EUROPEANRUSSIA","RUSSIA"},{"ASIATICRUSSIA","RUSSIA"},{"ASIANRUSSIA","RUSSIA"},
  {"EUROPEANTURKEY","TURKEY"},{"ASIATICTURKEY","TURKEY"},{"ASIANTURKEY","TURKEY"},
  {"TURKIYE","TURKEY"},{"TRKIYE","TURKEY"},{"REPUBLICOFTURKIYE","TURKEY"},{"REPUBLICOFTRKIYE","TURKEY"},
  {"WESTMALAYSIA","MALAYSIA"},{"EASTMALAYSIA","MALAYSIA"},
  {"DEMREPOFTHECONGO","DEMOCRATICREPUBLICOFTHECONGO"},
  {"BOSNIAHERZEGOVINA","BOSNIAANDHERZEGOVINA"},{"TRINIDADTOBAGO","TRINIDADANDTOBAGO"},
  {"REPUBLICOFKOREA","SOUTHKOREA"},{"DPRKOREA","NORTHKOREA"},{"DPROFKOREA","NORTHKOREA"},
  {"COTEDIVOIRE","IVORYCOAST"},{"CTEDIVOIRE","IVORYCOAST"},
  {"BURMA","MYANMAR"},{"SWAZILAND","ESWATINI"},{"MACEDONIA","NORTHMACEDONIA"}
 })if(auto found=distanceKeys_.find(pair.second);found!=distanceKeys_.end())distanceKeys_.emplace(pair.first,found->second);
 return ready();
}
bool LocationCheck::mismatch(const std::string& country,const std::string& grid,const std::string& call,double toleranceKm){
 // An opaque sender hash cannot support a callsign-country check. This is a
 // reversible confidence filter, not proof that the reception is false.
 // Bracketed full callsigns (e.g. WSPR <K1ABC>) are not opaque hashes.
 if(call=="<...>"||(call.size()>=5&&call.front()=='<'&&call.back()=='>'&&call[1]>='0'&&call[1]<='2'&&call[2]==':'&&std::all_of(call.begin()+3,call.end()-1,[](unsigned char c){return c>='0'&&c<='9';})))return true;
 // No country-centroid guesses, hash reuse or assumptions about portable calls.
 if(country.empty()||call.empty()||call.front()=='<'||call.find('/')!=std::string::npos)return false;
 auto point=gridPoint(grid);auto found=keys_.find(countryOutlineKey(country));if(!point||found==keys_.end())return false;
 toleranceKm=std::isfinite(toleranceKm)?std::clamp(toleranceKm,0.,2000.):300.;
 auto key=countryOutlineKey(country)+":"+upper(grid)+":"+std::to_string(toleranceKm);if(auto cached=cache_.find(key);cached!=cache_.end())return cached->second;
 double lon=grid.size()>=8?1./120:grid.size()>=6?1./12:2.,lat=lon/2;
 const double tolerance=toleranceKm+distanceBearing(*point,{std::clamp(point->lat+lat/2,-90.,90.),point->lon+lon/2}).first;
 bool mismatch=true;const auto& area=areas_[found->second];for(size_t index=0;index<area.polygons.size();index++){const auto& polygon=area.polygons[index];const auto& bounds=area.bounds[index];const double margin=tolerance/radius/rad+1;
  // Conservative geographic envelopes discard distant islands/continents
  // before any vertex scan. Skip longitude pruning near poles/dateline.
  if(point->lat<bounds.first.lat-margin||point->lat>bounds.second.lat+margin)continue;
  const double latitude=std::max({std::abs(point->lat),std::abs(bounds.first.lat),std::abs(bounds.second.lat)})+margin;
  if(latitude<89&&bounds.second.lon-bounds.first.lon<180){const double longitudeMargin=margin/std::cos(latitude*rad);if(point->lon<bounds.first.lon-longitudeMargin||point->lon>bounds.second.lon+longitudeMargin)continue;}
  if(insidePolygon(*point,polygon)){mismatch=false;break;}
  // Include all islands and border segments, with great-circle edge distance.
  for(const auto& ring:polygon){for(size_t i=0,j=ring.size()?ring.size()-1:0;i<ring.size();j=i++)if(edgeDistance(*point,ring[j],ring[i])<=tolerance){mismatch=false;break;}if(!mismatch)break;}if(!mismatch)break;
 }
 if(cache_.size()>=65536)cache_.clear();cache_[key]=mismatch;return mismatch;
}
}
