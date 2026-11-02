// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "temporary_folder.h"
#include <iostream>
#include <set>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(int argc,char** argv){
 CHECK(argc==3);auto pattern=(std::filesystem::temp_directory_path()/"dx-activity-estimates-XXXXXX").string();CHECK(mkdtemp(pattern.data()));TestFolderCleanup cleanup(pattern);auto root=std::filesystem::path(pattern);
 std::filesystem::create_directory(root/"profile");std::filesystem::copy_file(argv[1],root/"world.geojson");std::filesystem::create_symlink(std::filesystem::absolute(argv[2]),root/"dx_mock_valid_small");
 const auto home=*dx::gridPoint("KM69");const int64_t now=time(nullptr);auto country=[](const std::string&){return std::make_pair("Germany","EU");};
 std::vector<std::pair<std::string,std::string>> modes={{"FT8","DL1AAA"},{"FT4","DL2AAA"},{"WSPR","DL3AAA"},{"FST4","DL4AAA"},{"FST4W","DL5AAA"},{"Q65","DL6AAA"},{"JS8","DL7AAA"}};
 std::vector<dx::Json> original;
 {
  dx::Store store(root/"profile/history.sqlite3");
  auto insert=[&](int age,int64_t dial,const std::string& mode,const std::string& message){dx::Decode d;d.utc=now-age;d.slotMs=d.utc*1000;d.dial=dial;d.mode=mode;d.snr=-12;d.message=message;return store.insert(d,home,country);};
  for(const auto& [mode,call]:modes){auto message=mode=="JS8"?call+": @ALLCALL CQ":mode=="WSPR"||mode=="FST4W"?call+" 37":"K1AAA "+call+" -10";CHECK(insert(60,7074000,mode,message));}
  CHECK(insert(90,7074000,"FT4","CQ DL8AAA JO33"));CHECK(insert(30,7074000,"FT4","K1AAA DL8AAA RR73"));
  CHECK(insert(1200,7074000,"FT4","K1AAA DL9AAA -10")); // Outside an explicit 15-minute filter.
  CHECK(insert(60,14074000,"FT4","K1AAA DL0AAA -10")); // Different shared band filter.
  CHECK(insert(60,7074000,"FT4","CQ <1:123>")); // Unresolved sender must stay unknown.
  store.setSetting("ui",{{"history_all",false},{"direction_stations",25}});
  store.rebaseDistances(home);store.reclassifyCountries(country);for(const auto& [id,d]:store.logRows(0))original.push_back(d.json());
 }
 dx::Engine engine(root/"profile",root/"dx_mock_valid_small",home,country);uint64_t generation=20;
 auto check=[&](dx::Query query,const std::set<std::string>& expected){
  const auto requested=++generation;engine.query(query,query,requested);auto until=dx::monotonicMs()+10000;
  while(dx::monotonicMs()<until){auto s=engine.snapshot();if(s->generation==requested){
   std::set<std::string> actual;for(const auto& d:s->directionStations)actual.insert(d.identity.transmitter);
   if(actual!=expected){std::cerr<<"Unexpected sample:";for(const auto& call:actual)std::cerr<<' '<<call;std::cerr<<'\n';return false;}
   const auto bearing=dx::activityBearing(s->directionStations);return s->direction.stations==int(expected.size())&&s->direction.degrees==bearing.degrees;
  }std::this_thread::sleep_for(std::chrono::milliseconds(10));}return false;
 };
 dx::Query q;q.since=now-900;q.filter.band="40m";CHECK(check(q,{}));engine.settings({{"history_all",true}});
 std::set<std::string> all{"DL8AAA"};for(const auto& [mode,call]:modes)all.insert(call);CHECK(check(q,all));
 CHECK(engine.snapshot()->retention.firstUtc==now-1200&&engine.snapshot()->statsHistory);
 CHECK(!dx::historySpanText(engine.snapshot()->statsHistory,engine.snapshot()->retention.firstUtc,now).empty());
 CHECK(engine.snapshot()->direction.countryEstimates==7&&dx::activityBearingText(engine.snapshot()->direction).find("[C]")==std::string::npos);
 for(const auto& d:engine.snapshot()->directionStations){CHECK(d.bearing&&d.km);if(d.identity.transmitter=="DL8AAA")CHECK(d.countryDistanceGrid.empty()&&d.position&&d.gridSource=="cached");else{CHECK(!d.countryDistanceGrid.empty()&&!d.position&&d.identity.grid.empty());auto expected=dx::distanceBearing(home,*dx::gridPoint(d.countryDistanceGrid));CHECK(std::abs(*d.bearing-expected.second)<1e-9&&std::abs(*d.km-expected.first)<1e-9);}}
 for(const auto& row:engine.snapshot()->table)if(!row.decode.countryDistanceGrid.empty())CHECK(!row.decode.bearing&&!row.decode.position);
 for(const auto& [mode,call]:modes){q.filter.mode=mode;std::set<std::string> expected{call};if(mode=="FT4")expected.insert("DL8AAA");CHECK(check(q,expected));}
 q.filter.mode="FT4";q.filter.hasGrid=true;CHECK(check(q,{"DL8AAA"}));q.filter.hasGrid=false;
 engine.settings({{"approximate_country_distances",false}});CHECK(check(q,{"DL8AAA"}));CHECK(engine.snapshot()->direction.countryEstimates==0&&dx::activityBearingText(engine.snapshot()->direction).rfind("[C] ",0)!=0);
 engine.settings({{"approximate_country_distances",true}});CHECK(check(q,{"DL2AAA","DL8AAA"}));
 q.filter.search="DL2AAA";CHECK(check(q,{"DL2AAA"}));q.filter.search.clear();q.filter.minSnr=-10;CHECK(check(q,{}));q.filter.minSnr=-100;
 q.since=now-45;CHECK(check(q,{"DL8AAA"}));q.since=now-900;q.until=now-1200;CHECK(check(q,{}));q.until=INT64_MAX;
 CHECK(dx::activityBearingText(engine.snapshot()->direction)=="No matching stations");
 q.filter.band="20m";CHECK(check(q,{"DL0AAA"}));q.filter.band="40m";
 engine.settings({{"history_all",false},{"stats_history",true}});CHECK(check(q,{}));CHECK(engine.snapshot()->statsHistory&&engine.snapshot()->retention.firstUtc==now-1200);
 engine.settings({{"stats_history",false}});CHECK(check(q,{}));CHECK(!engine.snapshot()->statsHistory);
 engine.settings({{"history_all",true}});CHECK(check(q,{"DL2AAA","DL8AAA"}));
 auto before=engine.snapshot()->savedMessages;engine.active(true);dx::Job job;job.mode="FT4";job.dial=7074000;job.utcMs=now*1000;job.deadlineMs=dx::monotonicMs()+30000;job.pcm.resize(90000);engine.enqueue(std::move(job));
 auto until=dx::monotonicMs()+10000;while(dx::monotonicMs()<until&&engine.snapshot()->savedMessages<before+3)std::this_thread::sleep_for(std::chrono::milliseconds(10));
 CHECK(check(q,{"DL2AAA","DL8AAA","K0ABC","K1ABC","K2ABC"}));
 dx::Store saved(root/"profile/history.sqlite3");size_t index=0;for(const auto& [id,d]:saved.logRows(0)){if(index<original.size())CHECK(d.json()==original[index]);++index;}CHECK(index==original.size()+3);
 std::cout<<"All modes use home-based received/cached or country-estimated bearings; shared filters, history toggle, live arrivals and original evidence preserved\n";
}
