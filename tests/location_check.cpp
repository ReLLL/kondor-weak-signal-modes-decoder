// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include "engine.h"
#include "temporary_folder.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int unresolvedSenderFilter(const std::filesystem::path& world,const std::filesystem::path& helper){
 auto folder=(std::filesystem::temp_directory_path()/"dx-unresolved-filter-XXXXXX").string();CHECK(mkdtemp(folder.data()));TestFolderCleanup cleanup(folder);
 dx::Store store(std::filesystem::path(folder)/"history.sqlite3");CHECK(store.loadCountryAreas(world));auto home=*dx::gridPoint("KM69");
 auto reception=dx::Decode::fromJson(dx::Json::parse(R"({"audio_hz":823,"bearing":236.18152611882613,"called":"<1:2303>","continent":"","country":"","distance_record":true,"dt":-0.2,"first_call":false,"first_country":false,"first_grid":true,"frequency":14080000,"grid":"CE81WW","grid_source":"received","grid_utc":1792516470,"identity_source":"literal","km":17815.923154631204,"message":"<1:2303> <2:675541> R 570661 CE81WW","mode":"FT4","slot_ms":1792516470000,"snr":-20,"source":"live","timestamp":1792516470,"transmitter":"<2:675541>","type":"Grid"})"));
 CHECK(store.insert(reception,home));CHECK(reception.identity.transmitter=="<2:675541>"&&reception.country.empty()&&reception.km&&*reception.km>17000);
 const auto saved=reception.json();dx::Query query;query.messages=true;CHECK(store.rows(query).size()==1);
 query.filter.removeLikelyFalse=true;CHECK(store.rows(query).empty());int64_t count=-1;store.rows(query,0,0,&count);CHECK(count==0);
 CHECK(!store.passesLocationFilter(reception,query.filter));CHECK(store.stats(reception.utc,query.filter).messages==0&&store.stats(reception.utc,query.filter).farthest==0);
 CHECK(store.sessionStats(reception.utc,0,0,query.filter).messages==0&&store.sessionStats(reception.utc,0,0,query.filter).farthest==0);
 query.messages=false;query.uniqueCalls=true;CHECK(store.rows(query).empty());CHECK(store.rows(dx::activityQuery(query,reception.utc)).empty());
 // New unresolved receptions follow the same rule; filters never delete history.
 reception.utc+=15;reception.slotMs=reception.utc*1000;CHECK(store.insert(reception,home));CHECK(store.rows(query).empty()&&store.stats(reception.utc,query.filter).messages==0);
 CHECK(store.savedMessageCount()==2&&store.ignoredCalls().empty()&&store.ignoredMessages().empty());CHECK(store.logRows(0).front().second.json()==saved);
 query.filter.removeLikelyFalse=false;query.messages=true;CHECK(store.rows(query).size()==2&&store.stats(reception.utc).messages==2);
 // Toggle a running Engine with retained and newly decoded hash receptions.
 auto root=std::filesystem::path(folder)/"engine";std::filesystem::create_directory(root);auto mock=root/"dx_mock_valid_unresolved_small";std::filesystem::create_symlink(std::filesystem::absolute(helper),mock);std::filesystem::copy_file(world,root/"world.geojson");const auto current=int64_t(time(nullptr));
 {dx::Store seed(root/"history.sqlite3");auto old=reception;old.utc=current-60;old.slotMs=old.utc*1000;CHECK(seed.insert(old,home));seed.setSetting("ui",{{"history_all",true}});}
 {dx::Engine engine(root,mock,home,{});auto wait=[&](auto predicate){const auto until=dx::monotonicMs()+8000;while(dx::monotonicMs()<until){if(predicate(*engine.snapshot()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return predicate(*engine.snapshot());};
 CHECK(wait([](const auto& s){return s.historyAll&&s.savedMessages==1;}));dx::Query table,map;table.messages=true;map.uniqueCalls=true;table.filter.removeLikelyFalse=map.filter.removeLikelyFalse=true;engine.query(table,map,1);
 CHECK(wait([](const auto& s){return s.generation==1&&s.tableCount==0&&s.mapCount==0&&s.stats.messages==0&&s.stats.farthest==0&&s.directionStations.empty();}));
 engine.active(true);engine.alertContext("20m",false,true);dx::Job job;job.mode="FT4";job.dial=14080000;job.utcMs=current*1000;job.deadlineMs=dx::monotonicMs()+30000;job.pcm.resize(90000);engine.enqueue(job);
 CHECK(wait([](const auto& s){return s.decodedJobs==1&&s.savedMessages==2&&s.pendingWrites==0&&!s.csvCatchingUp;}));CHECK(engine.snapshot()->stats.messages==0&&engine.snapshot()->stats.farthest==0&&engine.snapshot()->tableCount==0&&engine.snapshot()->mapCount==0&&engine.snapshot()->directionStations.empty()&&engine.snapshot()->alert.empty()&&engine.snapshot()->cornerReceptions.empty());
 table.filter.removeLikelyFalse=map.filter.removeLikelyFalse=false;engine.query(table,map,2);CHECK(wait([](const auto& s){return s.generation==2&&s.tableCount==2&&s.mapCount==1&&s.stats.messages==2&&s.stats.farthest>17000;}));
 table.messages=false;table.uniqueCalls=true;engine.query(table,map,3);CHECK(wait([](const auto& s){return s.generation==3&&s.tableCount==1;}));
 table.filter.removeLikelyFalse=map.filter.removeLikelyFalse=true;engine.query(table,map,4);CHECK(wait([](const auto& s){return s.generation==4&&s.tableCount==0&&s.mapCount==0&&s.stats.messages==0;}));
 table.countries=true;engine.query(table,map,5);CHECK(wait([](const auto& s){return s.generation==5&&s.countryTable.empty()&&s.stats.farthest==0;}));
 }
 {dx::Store savedStore(root/"history.sqlite3");CHECK(savedStore.savedMessageCount()==2&&savedStore.logRows(0).size()==2&&savedStore.ignoredCalls().empty());}
 std::ifstream csv(root/"receptions.csv");std::string line;int lines=0;while(std::getline(csv,line))++lines;CHECK(lines==3);
 return 0;
}
int main(int argc,char** argv){
 CHECK(argc==3);CHECK(unresolvedSenderFilter(argv[1],argv[2])==0);
 dx::LocationCheck check;CHECK(check.load(argv[1]));CHECK(!check.mismatch("Germany","JO33","DL8OH"));CHECK(check.mismatch("Germany","QF56","DL1ABC"));CHECK(!check.mismatch("Germany","QF56","DL1ABC/VK"));CHECK(!check.mismatch("United States","FN42","K1ABC"));CHECK(!check.mismatch("European Russia","KO85","RA5AD"));CHECK(!check.mismatch("Unknown country","QF56","DL1ABC"));CHECK(!check.mismatch("Germany","","DL1ABC"));CHECK(check.mismatch("Germany","QF56","<1:123>"));
 for(const auto& call:{"<0:1>","<1:2303>","<2:675541>","<...>"})CHECK(check.mismatch("","",call));
 for(const auto& call:{"","K1ABC","K1ABC/P","<K1ABC>","<1:>","<1:ABC>"})CHECK(!check.mismatch("","CE81WW",call));
 auto folder=(std::filesystem::temp_directory_path()/"dx-location-check-XXXXXX").string();CHECK(mkdtemp(folder.data()));TestFolderCleanup cleanup(folder);dx::Store store(std::filesystem::path(folder)/"history.sqlite3");CHECK(store.loadCountryAreas(argv[1]));auto home=*dx::gridPoint("KM69");
 int64_t utc=1800000000;for(const auto& message:{"CQ DL8OH JO33","CQ DL1ABC QF56","JA1ABC DL1ABC -10","CQ DL1ABC/VK QF56"}){dx::Decode d;d.utc=utc++;d.mode="FT8";d.dial=14074000;d.message=message;CHECK(store.insert(d,home,[](const auto&){return std::make_pair("Germany","EU");}));}
 dx::Query q;q.messages=true;CHECK(store.rows(q).size()==4);q.filter.removeLikelyFalse=true;CHECK(dx::Filter::fromJson(q.filter.json()).removeLikelyFalse);auto rows=store.rows(q);CHECK(rows.size()==2);int64_t count=0;store.rows(q,0,0,&count);CHECK(count==2);CHECK(store.stats(utc).messages==4&&store.savedMessageCount()==4);q.filter.removeLikelyFalse=false;CHECK(store.rows(q).size()==4&&store.ignoredCalls().empty());
 auto filter=q.filter;filter.removeLikelyFalse=true;
 auto filtered=store.stats(utc,filter);CHECK(filtered.messages==2&&filtered.stations==2&&filtered.countries==1&&filtered.recentStations==2);CHECK(filtered.farthestCall=="DL1ABC/VK");CHECK(filtered.latest.size()==2&&filtered.heardCountries==std::vector<std::string>{"Germany"});
 auto session=store.sessionStats(utc,0,utc-100,filter);CHECK(session.messages==2&&session.stations==2&&session.recentStations==2);CHECK(store.sessionStats(utc,0,utc-100).messages==4);CHECK(store.sessionStats(utc,0,utc-100,filter).messages==2);
 // New receptions use the same policy without restarting. Unknown evidence is kept.
 for(const auto& message:{"CQ F1BAD QF56","CQ F1GOOD JN18"}){dx::Decode d;d.utc=utc++;d.source="live";d.mode="FT8";d.dial=14074000;d.message=message;CHECK(store.insert(d,home,[](const auto&){return std::make_pair("France","EU");}));}
 filtered=store.stats(utc,filter);CHECK(filtered.messages==3&&filtered.stations==3&&filtered.countries==2&&filtered.recentStations==3);CHECK(filtered.latest.front().identity.transmitter=="F1GOOD");CHECK(filtered.newestCountries.front()=="France");CHECK(store.sessionStats(utc,0,utc-100,filter).messages==3);
 q.filter=filter;q.uniqueCalls=true;q.messages=false;CHECK(store.rows(q).size()==3);q.messages=true;CHECK(store.rows(q).size()==3);CHECK(store.savedMessageCount()==6&&store.stats(utc).messages==6);CHECK(store.sessionStats(utc,0,utc-100,filter).recentStations==3);
 auto loose=filter;loose.falseToleranceKm=2000;CHECK(store.stats(utc,loose).messages==3);CHECK(store.stats(utc+400,filter).recentStations==0);
 // Incremental retention must refresh both caches, without counting rejected rows.
 CHECK(store.retain(5,1,INT64_MAX,3)==1);CHECK(store.stats(utc,filter).messages==2);CHECK(store.sessionStats(utc,0,utc-100,filter).messages==2);
 CHECK(store.retain(5,1,INT64_MAX,3)==1);CHECK(store.stats(utc,filter).messages==2);CHECK(store.sessionStats(utc,0,utc-100,filter).messages==2);
 CHECK(store.retain(5,1,INT64_MAX,3)==1);CHECK(store.stats(utc,filter).messages==2&&store.savedMessageCount()==3);CHECK(store.sessionStats(utc,0,utc-100,filter).messages==2);
 CHECK(store.retain(2,1,INT64_MAX,1)==1);filtered=store.stats(utc,filter);CHECK(filtered.messages==1&&filtered.countries==1&&filtered.stations==1&&filtered.farthestCall=="F1GOOD"&&filtered.newestCountries==std::vector<std::string>{"France"});CHECK(store.sessionStats(utc,0,utc-100,filter).farthestCall=="F1GOOD");
 CHECK(store.retain(2,1,INT64_MAX,1)==1);CHECK(store.stats(utc,filter).messages==1&&store.savedMessageCount()==1);
 // Toggle the actual running Engine: past entries, new writes, all table tabs,
 // saved-statistics scope, reversible views, CSV preservation, no restart.
 auto root=std::filesystem::path(folder)/"engine";std::filesystem::create_directory(root);auto mock=root/"dx_mock_valid_small";std::filesystem::create_symlink(std::filesystem::absolute(argv[2]),mock);std::filesystem::copy_file(argv[1],root/"world.geojson");auto current=int64_t(time(nullptr));
 auto country=[](const auto&){return std::make_pair(std::string("Germany"),std::string("EU"));};
 {dx::Store seed(root/"history.sqlite3");for(const auto& message:{"CQ DL8OH JO33","CQ DL1ABC QF56"}){dx::Decode d;d.utc=current-30;d.mode="FT8";d.dial=14074000;d.message=message;d.source="live";CHECK(seed.insert(d,home,country));}seed.setSetting("ui",{{"history_all",true}});}
 {dx::Engine engine(root,mock,home,country);auto wait=[&](auto predicate){auto until=dx::monotonicMs()+8000;while(dx::monotonicMs()<until){if(predicate(*engine.snapshot()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}auto s=engine.snapshot();std::cerr<<"Engine: "<<s->error<<" generation "<<s->generation<<" stats "<<s->stats.messages<<" saved "<<s->savedMessages<<'\n';return predicate(*s);};
 CHECK(wait([](const auto& s){return s.historyAll&&s.stats.messages==2;}));dx::Query table,map;table.messages=true;map.uniqueCalls=true;table.filter=map.filter=filter;engine.query(table,map,10);
 CHECK(wait([](const auto& s){return s.generation==10&&s.tableCount==1&&s.mapCount==1&&s.stats.messages==1&&s.stats.stations==1&&s.stats.recentStations==1&&s.stats.farthestCall=="DL8OH";}));
 engine.active(true);engine.alertContext("20m",true,false);dx::Job job;job.mode="FT8";job.dial=14074000;job.utcMs=current*1000;job.deadlineMs=dx::monotonicMs()+30000;job.pcm.resize(180000);engine.enqueue(job);
 CHECK(wait([](const auto& s){return s.decodedJobs==1&&s.savedMessages==5&&s.pendingWrites==0&&!s.csvCatchingUp;}));CHECK(engine.snapshot()->stats.messages==1&&engine.snapshot()->tableCount==1&&engine.snapshot()->mapCount==1&&engine.snapshot()->alert.empty()&&engine.snapshot()->cornerReceptions.empty());
 table.filter.removeLikelyFalse=map.filter.removeLikelyFalse=false;engine.query(table,map,11);CHECK(wait([](const auto& s){return s.generation==11&&s.stats.messages==5&&s.tableCount==5&&s.mapCount==5;}));
 table.filter=map.filter=filter;table.messages=false;table.countries=true;engine.query(table,map,12);CHECK(wait([](const auto& s){return s.generation==12&&s.countryTable.size()==1&&s.countryTable.front().stations==1&&s.stats.messages==1;}));
 engine.settings({{"history_all",false},{"stats_history",true}});CHECK(wait([](const auto& s){return !s.historyAll&&s.statsHistory&&s.stats.messages==1&&s.mapCount==0;}));CHECK(engine.snapshot()->savedMessages==5);
 }
 {dx::Store saved(root/"history.sqlite3");CHECK(saved.savedMessageCount()==5&&saved.ignoredCalls().empty());CHECK(saved.logRows(0,100).size()==5);}
 std::ifstream csv(root/"receptions.csv");std::string line;int lines=0;while(std::getline(csv,line))++lines;CHECK(lines==6);
 std::cout<<"Country outlines, backward/live filtering, incremental filtered stats and retention preserve history\n";
}
