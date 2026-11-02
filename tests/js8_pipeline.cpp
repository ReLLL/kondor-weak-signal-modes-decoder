// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include <fstream>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
int main(int argc,char** argv){
 if(argc!=2)return 2;auto root=std::filesystem::path(argv[1]);CHECK(std::filesystem::is_regular_file(root/".js8-test-owned"));std::ifstream input(root/"frames.json");dx::Json fixture;input>>fixture;auto home=*dx::gridPoint("KM69");int expected=3;
 {
  dx::Engine engine(root/"profile",root/"dx_mock_valid_small",home,[](const auto& call){return call.rfind("DL",0)==0?std::make_pair("Germany","EU"):std::make_pair("United States","NA");});
  // The isolated helper has a 20-second response limit. Allow that failure to
  // reach the snapshot even when concurrent decoder tests delay cold startup.
  auto wait=[&](auto condition){auto until=dx::monotonicMs()+25000;while(dx::monotonicMs()<until){if(condition(*engine.snapshot()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}std::cerr<<engine.snapshot()->js8Error<<" | "<<engine.snapshot()->error<<'\n';return condition(*engine.snapshot());};
  CHECK(wait([](const auto& s){return s.generation>0;}));engine.active(true);dx::Job ft8;ft8.mode="FT8";ft8.dial=14074000;ft8.utcMs=int64_t(time(nullptr))*1000;ft8.deadlineMs=dx::monotonicMs()+30000;ft8.pcm.resize(180000);engine.enqueue(ft8);CHECK(wait([](const auto& s){return s.stats.messages==3;}));
  // A malformed JS8 job kills only that helper; a subsequent frame restarts it.
  auto broken=ft8;broken.mode="JS8";broken.periodMs=5000;broken.pcm.resize(60000);engine.enqueue(broken);CHECK(wait([](const auto& s){return s.failures>0&&!s.js8Error.empty();}));CHECK(engine.snapshot()->stats.messages==3&&engine.snapshot()->error.empty());
  int period=fixture["period"];int64_t stamp=int64_t(time(nullptr))+1;uint64_t jobs=engine.snapshot()->decodedJobs;
  for(size_t i=0;i<fixture["frames"].size();i++){
   auto file=root/("frame-"+std::to_string(i)+".pcm");std::ifstream audio(file,std::ios::binary);dx::Job job;job.mode="JS8";job.dial=7078000;job.periodMs=period*1000;job.utcMs=(stamp+i*period)*1000;job.deadlineMs=dx::monotonicMs()+30000;job.source="fixture";job.pcm.resize(size_t(period)*12000);CHECK(audio.read(reinterpret_cast<char*>(job.pcm.data()),job.pcm.size()*2));engine.enqueue(std::move(job));++jobs;
   CHECK(wait([&](const auto& s){return s.decodedJobs>=jobs&&s.pendingWrites==0&&s.js8Error.empty();}));
  }
  CHECK(wait([](const auto& s){return !s.js8Decoding&&s.tableCount==4&&s.mapCount==4&&!s.csvCatchingUp;}));auto snapshot=engine.snapshot();CHECK(snapshot->stats.countries==2&&snapshot->stats.stations==4);auto found=std::find_if(snapshot->table.begin(),snapshot->table.end(),[](const auto& row){return row.decode.mode=="JS8";});CHECK(found!=snapshot->table.end());CHECK(found->decode.messageComplete&&found->decode.identity.transmitter=="DL8OH"&&found->decode.identity.called=="K1ABC"&&found->decode.country=="Germany"&&found->decode.identity.grid=="JO33"&&found->decode.km&&found->decode.gridSource=="cached");
  CHECK(found->decode.message.find("HELLO THIS IS A LONG MESSAGE TESTING THE JS8 RECEPTION PIPELINE")!=std::string::npos);expected=int(snapshot->savedMessages);CHECK(expected>4);
  dx::Query table,map;table.messages=true;table.filter.mode=map.filter.mode="JS8";engine.query(table,map,20);CHECK(wait([&](const auto& s){return s.generation==20&&s.tableCount==expected-3&&s.mapCount==1;}));CHECK(!engine.snapshot()->paths.empty());for(const auto& path:engine.snapshot()->paths)CHECK(path.transmitter=="DL8OH"&&path.called=="K1ABC");
 }
 dx::Store saved(root/"profile/history.sqlite3");CHECK(saved.savedMessageCount()==expected&&saved.ignoredCalls().empty());dx::Query query;query.messages=true;query.filter.mode="JS8";auto rows=saved.rows(query);CHECK(!rows.empty()&&rows.front().decode.messageComplete);CHECK(!rows.back().decode.raw.empty());std::cout<<"JS8 helper recovery, independent FT8 reception, country/grid, partial/final messages, map/table filtering and saved history passed\n";
}
