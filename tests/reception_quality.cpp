// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include "engine.h"
#include "csv_log.h"
#include "temporary_folder.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
namespace {
dx::Decode heard(int second,const std::string& message,const std::string& flags="0",const std::string& hints="CQ;K1ABC;"){
 auto raw="FT8_OUT\t1800000000000\t00\t{0}\t120000\t{1}\t-24\t{2}\t0.1\t{3}\t0\t{4}\t"+message+"|"+hints+"\t{5}\t"+flags+"\t{6}\t"+(flags.find('?')!=std::string::npos?"0.1":"0.9")+"\t{7}\t1000\n";
 return dx::Decode::fromJson({{"timestamp",1800000000+second},{"frequency",7074000},{"mode","FT8"},{"snr",-24},{"audio_hz",1000},{"message",message},{"raw",raw},{"source","live"}});
}
}
int main(int argc,char** argv){
 CHECK(dx::operatingCall("BT4QMQ/R")=="BT4QMQ");CHECK(dx::operatingCall("W1AW/F")=="F");CHECK(dx::operatingCall("F/K1ABC")=="F");CHECK(dx::operatingCall("K1ABC/VE3")=="VE3");CHECK(dx::operatingCall("<A60FD/5>")=="A60FD");
 // This exact free-text reception had been assigned a Romanian station/cache.
 auto text=heard(0,"S93 YO2MHF KN","0",";;");CHECK(text.identity.transmitter.empty()&&text.identity.grid.empty());
 CHECK(dx::meaning("S93 YO2MHF KN").transmitter.empty());
 CHECK(heard(0,"CQ K1ABC FN42","0",";;").identity.transmitter.empty());
 CHECK(heard(0,"K1ABC K2ABC -10","0",";;").identity.transmitter.empty());
 auto low=heard(15,"CQ K1BAD AA00","? 0","CQ;K1BAD;");CHECK(low.json().value("decoder_suspect",false));
 auto parsed=dx::parseDecoderOutput("FT8",low.raw);CHECK(parsed&&parsed->value("decoder_suspect",false)&&parsed->at("decoder_flags")=="? 0");
 auto invalid=low.raw;invalid.replace(invalid.find("-24"),3,"-24junk");CHECK(!dx::parseDecoderOutput("FT8",invalid));CHECK(!dx::parseDecoderOutput("FT4",low.raw));
 auto weak=heard(0,"CQ K1ABC FN42");CHECK(!weak.json().value("decoder_suspect",false));
 auto inspect=dx::Filter::fromJson({{"includeQuestionable",true}});CHECK(inspect.json().value("includeQuestionable",false));
 CHECK(!dx::Filter{}.accepts(low)&&inspect.accepts(low));
 auto bracket=heard(30,"K2ABC <K1ABC> -10","0","K2ABC;<K1ABC>;");CHECK(bracket.identity.transmitter=="K1ABC");
 auto special=heard(30,"<OR80AF> LY3CY KO24","0","<OR80AF>;LY3CY;");CHECK(special.identity.transmitter=="LY3CY"&&special.identity.grid=="KO24");
 for(const auto& message:{"WA9XYZ KA1ABC 16A EMA","WA9XYZ KA1ABC R 16A EMA","TU; WA9XYZ KA1ABC 579 MA","TU; WA9XYZ KA1ABC R 579 1234","WA9XYZ KA1ABC 571671 FN42AB","WA9XYZ KA1ABC R 571671 FN42AB","<OR80AF> LY3CY RR73","CQ DX OR80AF JO10"})CHECK(!dx::meaning(message).transmitter.empty());
 auto unknown=heard(30,"<1:2303> <2:675541> R 570661 CE81WW","? 0","<1:2303>;<2:675541>;");CHECK(unknown.identity.transmitter=="<2:675541>");
 CHECK(dx::meaning("VN9IRZ ML2DDC 14H VT").transmitter=="ML2DDC");
 auto folder=(std::filesystem::temp_directory_path()/"dx-quality-XXXXXX").string();CHECK(mkdtemp(folder.data()));TestFolderCleanup cleanup(folder);
 dx::Store store(std::filesystem::path(folder)/"history.sqlite3");auto home=*dx::gridPoint("KM69");
 auto country=[](const std::string&){return std::make_pair("United States","NA");};
 CHECK(store.insert(weak,home,country));CHECK(store.insert(low,home,country));
 dx::Query q;q.messages=true;CHECK(store.rows(q).size()==1);int64_t count=-1;store.rows(q,0,0,&count);CHECK(count==1);
 CHECK(store.stats(low.utc).messages==1&&store.stats(low.utc).stations==1);CHECK(store.sessionStats(low.utc,0,0).messages==1);
 q.filter=inspect;CHECK(store.rows(q).size()==2&&store.stats(low.utc,inspect).messages==2&&store.savedMessageCount()==2);
 auto poisoned=heard(45,"CQ K1ABC DB80","? 0");CHECK(store.insert(poisoned,home,country));
 auto report=heard(60,"K2ABC K1ABC -10","0","K2ABC;K1ABC;");CHECK(store.insert(report,home,country));CHECK(report.identity.grid=="FN42"&&report.gridSource=="cached");
 auto hash="<2:"+std::to_string(dx::callHash("K1BAD",22))+">";
 auto hashReport=heard(75,"K2ABC "+hash+" -10","0","K2ABC;"+hash+";");CHECK(store.insert(hashReport,home,country));CHECK(hashReport.identity.transmitter==hash);
 // Inspecting questionable history must not change original stored evidence.
 auto original=store.logRows(0);CHECK(original.size()==5&&original[1].second.raw==low.raw);
 q.filter={};auto goodStats=store.stats(report.utc);CHECK(goodStats.stations==1&&goodStats.farthest<10000);
 CHECK(store.insert(text,home,country));CHECK(store.stats(report.utc).stations==1);
 CHECK(!low.distanceRecord&&!poisoned.distanceRecord);
 auto distant=heard(90,"CQ FK8HC RG37","0","CQ;FK8HC;");CHECK(dx::Filter{}.accepts(distant));
 auto confirmed=heard(105,"CQ K1BAD FN42","0","CQ;K1BAD;");CHECK(store.insert(confirmed,home,country)&&confirmed.firstCall);
 {
  auto oldFile=std::filesystem::path(folder)/"old.sqlite3";auto good=heard(0,"CQ K1AAA KN06"),bad=heard(15,"CQ K2AAA CE81","? 0");
  {dx::Store old(oldFile);CHECK(old.insert(good,home,country)&&old.insert(bad,home,country));old.setSetting("confidence_schema",0);old.setSetting("farthest_km",20000.);}
  {dx::Store reopened(oldFile);CHECK(reopened.savedMessageCount()==2&&reopened.getSetting("farthest_km",0.).get<double>()==*good.km);auto confirmed=heard(30,"CQ K2AAA FN42");CHECK(reopened.insert(confirmed,home,country)&&confirmed.firstCall&&confirmed.distanceRecord);reopened.rebaseDistances({40,34});CHECK(reopened.getSetting("farthest_km",0.).get<double>()==reopened.stats(confirmed.utc).farthest);}
 }
 {
  dx::Store ranked(":memory:");auto high=heard(0,"CQ K1AAA FN42"),questionable=heard(15,"CQ K2AAA FN42","? 0"),legacy=high;legacy.utc+=30;legacy.mode="WSPR";legacy.message="K3AAA FN42 30";legacy.raw.clear();legacy.diagnostics={};
  CHECK(ranked.insert(high,home,country)&&ranked.insert(questionable,home,country)&&ranked.insert(legacy,home,country));dx::Query query;query.messages=true;query.filter.includeQuestionable=true;query.sort=20;
  for(bool descending:{false,true}){query.descending=descending;auto rows=ranked.rows(query);CHECK(rows.size()==3&&!rows.back().decode.diagnostics.quality);CHECK(rows.front().decode.identity.transmitter==(descending?"K1AAA":"K2AAA"));}
  auto& range=query.filter.columnRanges[size_t(dx::ColumnMetric::Confidence)];range={.8,1};auto filtered=ranked.rows(query);CHECK(filtered.size()==1&&filtered.front().decode.identity.transmitter=="K1AAA");CHECK(dx::Filter::fromJson(query.filter.json()).json()==query.filter.json());CHECK(!query.filter.accepts(legacy)&&query.filter.accepts(high));int64_t total=0;ranked.rows(query,0,0,&total);CHECK(total==1);
  CHECK(dx::confidenceSummary(high)=="0.9"&&dx::confidenceSummary(questionable)=="Low 0.1"&&dx::confidenceSummary(legacy)=="--");
 }
 if(argc==2){
  auto root=std::filesystem::path(folder)/"engine";std::filesystem::create_directory(root);auto helper=root/"dx_mock_valid_questionable";std::filesystem::create_symlink(std::filesystem::absolute(argv[1]),helper);auto now=int64_t(time(nullptr));
  {dx::Store seed(root/"history.sqlite3");auto good=heard(0,"CQ K1ABC FN42");good.utc=now-60;CHECK(seed.insert(good,home,country));seed.setSetting("ui",{{"history_all",true},{"csv_path",(root/"receptions.csv").string()}});}
  {
   dx::Engine engine(root,helper,home,country);auto wait=[&](auto predicate){auto until=dx::monotonicMs()+8000;while(dx::monotonicMs()<until){if(predicate(*engine.snapshot()))return true;std::this_thread::sleep_for(std::chrono::milliseconds(10));}return predicate(*engine.snapshot());};
   CHECK(wait([](const auto& s){return s.historyAll&&s.stats.stations==1&&s.savedMessages==1;}));dx::Query table,map;table.messages=true;map.uniqueCalls=true;engine.query(table,map,1);engine.active(true);engine.alertContext("40m",true,false);
   dx::Job job;job.mode="FT8";job.dial=7074000;job.utcMs=now*1000;job.deadlineMs=dx::monotonicMs()+30000;job.pcm.resize(180000);engine.enqueue(job);
   CHECK(wait([](const auto& s){return s.decodedJobs==1&&s.savedMessages==2&&s.pendingWrites==0&&!s.csvCatchingUp;}));CHECK(engine.snapshot()->stats.stations==1&&engine.snapshot()->stats.messages==1&&engine.snapshot()->alert.empty()&&engine.snapshot()->cornerReceptions.empty());
   table.filter.includeQuestionable=map.filter.includeQuestionable=true;engine.query(table,map,2);CHECK(wait([](const auto& s){return s.generation==2&&s.tableCount==2&&s.mapCount==1&&s.stats.messages==1&&s.stats.stations==1;}));
   CHECK(engine.snapshot()->alert.empty()&&engine.snapshot()->cornerReceptions.empty());
   table.filter.includeQuestionable=map.filter.includeQuestionable=false;engine.query(table,map,3);CHECK(wait([](const auto& s){return s.generation==3&&s.tableCount==1&&s.stats.messages==1;}));
  }
  {dx::Store verify(root/"history.sqlite3");CHECK(verify.savedMessageCount()==2&&verify.logRows(0).size()==2);dx::CsvLog csv;CHECK(!csv.sync(verify,root/"receptions.csv",1024*1024)&&csv.cursor()==2);}
 }
 std::cout<<"Decoder flags, raw free text, bracketed calls, view/statistics policy and cache isolation passed\n";
}
