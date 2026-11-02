// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <set>
#include <cmath>
namespace dx {
namespace {
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db,const std::string& sql){sqlite3_stmt* s=nullptr;if(sqlite3_prepare_v2(db,sql.c_str(),-1,&s,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));return {s,sqlite3_finalize};}
int step(sqlite3* db,sqlite3_stmt* s){int rc=sqlite3_step(s);if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));return rc;}
std::string text(sqlite3_stmt* s,int i){auto p=sqlite3_column_text(s,i);return p?reinterpret_cast<const char*>(p):"";}
Decode decode(sqlite3_stmt* s,int i=0){return Decode::fromJson(Json::parse(text(s,i)));}
}
Stats Store::stats(int64_t now,const Filter& filter){
 if(filter.removeLikelyFalse||!filter.includeQuestionable){auto out=scopedStats(now,0,0,filter,historyCache_,true);out.activeSeconds=getSetting("active_seconds",0.).get<double>();return out;}
 Stats out;auto countries=prepare(db_,"SELECT value FROM entities WHERE kind='country' ORDER BY value");while(step(db_,countries.get())==SQLITE_ROW)out.heardCountries.push_back(text(countries.get(),0));out.countries=out.heardCountries.size();
 auto calls=prepare(db_,"SELECT count(*) FROM entities WHERE kind='call' AND value NOT LIKE '<%'");if(step(db_,calls.get())==SQLITE_ROW)out.stations=sqlite3_column_int64(calls.get(),0);out.messages=savedMessageCount();
 auto recent=prepare(db_,"SELECT count(DISTINCT call) FROM decodes WHERE utc BETWEEN ? AND ? AND call!='' AND call NOT LIKE '<%'");sqlite3_bind_int64(recent.get(),1,now-300);sqlite3_bind_int64(recent.get(),2,now+30);if(step(db_,recent.get())==SQLITE_ROW)out.recentStations=sqlite3_column_int64(recent.get(),0);
 auto farthest=prepare(db_,"SELECT payload FROM decodes WHERE km IS NOT NULL AND json_extract(payload,'$.grid_source')='received' ORDER BY km DESC,utc DESC,reception_id DESC LIMIT 1");if(step(db_,farthest.get())==SQLITE_ROW){auto d=decode(farthest.get());out.farthest=d.km.value_or(0);out.farthestCountry=d.country;out.farthestCall=d.identity.transmitter;out.farthestReception=std::move(d);}
 auto latest=prepare(db_,"SELECT d.payload FROM entities AS e JOIN decodes AS d ON d.reception_id=e.last_id WHERE e.kind='country' ORDER BY e.last_utc DESC,e.last_id DESC");while(step(db_,latest.get())==SQLITE_ROW)out.latestCountries.push_back(decode(latest.get()));
 auto last=prepare(db_,"SELECT d.payload FROM entities AS e JOIN decodes AS d ON d.reception_id=e.last_id WHERE e.kind='call' AND e.value NOT LIKE '<%' ORDER BY e.last_utc DESC,e.last_id DESC LIMIT 5");while(step(db_,last.get())==SQLITE_ROW)out.latest.push_back(decode(last.get()));
 auto first=prepare(db_,"SELECT value FROM entities WHERE kind='country' ORDER BY first_id DESC LIMIT 3");while(step(db_,first.get())==SQLITE_ROW)out.newestCountries.push_back(text(first.get(),0));
 out.activeSeconds=getSetting("active_seconds",0.).get<double>();return out;
}
Stats Store::sessionStats(int64_t now,int64_t afterRowId,int64_t since,const Filter& filter){
 if(filter.includeQuestionable&&!filter.removeLikelyFalse&&afterRowId==0&&since==0){auto out=stats(now,filter);auto far=prepare(db_,"SELECT payload FROM decodes WHERE km IS NOT NULL ORDER BY km DESC,utc DESC,reception_id DESC LIMIT 1");if(step(db_,far.get())==SQLITE_ROW){auto d=decode(far.get());out.farthest=d.km.value_or(0);out.farthestCountry=d.country;out.farthestCall=d.identity.transmitter;out.farthestReception=std::move(d);}return out;}
 return scopedStats(now,afterRowId,since,filter,sessionCache_,false);
}
Stats Store::scopedStats(int64_t now,int64_t afterRowId,int64_t since,const Filter& filter,StatsCache& cache,bool receivedOnly){
 const auto tolerance=std::isfinite(filter.falseToleranceKm)?std::clamp(filter.falseToleranceKm,0.,2000.):300.;
 auto version=prepare(db_,"PRAGMA data_version");step(db_,version.get());auto current=sqlite3_column_int64(version.get(),0);
 if(cache.after!=afterRowId||cache.since!=since||cache.dataVersion!=current||cache.filtered!=filter.removeLikelyFalse||cache.tolerance!=tolerance||cache.receivedOnly!=receivedOnly||cache.includeQuestionable!=filter.includeQuestionable){cache={};cache.after=afterRowId;cache.since=since;cache.cursor=afterRowId;cache.dataVersion=current;cache.filtered=filter.removeLikelyFalse;cache.tolerance=tolerance;cache.receivedOnly=receivedOnly;cache.includeQuestionable=filter.includeQuestionable;}
 Filter location;location.includeQuestionable=cache.includeQuestionable;location.removeLikelyFalse=cache.filtered;location.falseToleranceKm=tolerance;
 auto read=prepare(db_,"SELECT reception_id,payload FROM decodes WHERE reception_id>? ORDER BY reception_id");sqlite3_bind_int64(read.get(),1,cache.cursor);
 // Only committed new arrivals are read. A failure invalidates the partial cache.
 try{while(step(db_,read.get())==SQLITE_ROW){int64_t id=sqlite3_column_int64(read.get(),0);auto d=decode(read.get(),1);cache.cursor=id;if((d.source=="legacy"&&d.utc<since)||!passesLocationFilter(d,location))continue;++cache.totals.messages;
  auto latest=[&](auto& entries,const std::string& key){if(key.empty()||key.front()=='<')return;auto found=entries.find(key);if(found==entries.end()||std::make_pair(d.utc,id)>std::make_pair(found->second.second.utc,found->second.first))entries[key]={id,d};};
  latest(cache.calls,d.identity.transmitter);latest(cache.countries,d.country);if(!d.country.empty())cache.countryFirst.emplace(d.country,id);
  auto& totals=cache.totals;if(d.km&&(!receivedOnly||d.gridSource=="received")&&(!totals.farthestReception||*d.km>totals.farthest||(*d.km==totals.farthest&&d.utc>=totals.farthestReception->utc))){totals.farthest=*d.km;totals.farthestCountry=d.country;totals.farthestCall=d.identity.transmitter;totals.farthestReception=d;cache.farthestId=id;}
 }}catch(...){cache.after=-1;throw;}
 Stats out=cache.totals;out.stations=cache.calls.size();out.countries=cache.countries.size();for(const auto& [country,value]:cache.countries){(void)value;out.heardCountries.push_back(country);}
 auto latest=[](const auto& cache,size_t limit){std::vector<const std::pair<int64_t,Decode>*> sorted;for(const auto& [key,value]:cache){(void)key;sorted.push_back(&value);}auto count=std::min(limit,sorted.size());std::partial_sort(sorted.begin(),sorted.begin()+count,sorted.end(),[](const auto* a,const auto* b){return std::make_pair(a->second.utc,a->first)>std::make_pair(b->second.utc,b->first);});std::vector<Decode> rows;for(size_t i=0;i<count;i++)rows.push_back(sorted[i]->second);return rows;};
 out.latest=latest(cache.calls,5);out.latestCountries=latest(cache.countries,cache.countries.size());
 std::vector<std::pair<int64_t,std::string>> first;for(const auto& [country,id]:cache.countryFirst)first.emplace_back(id,country);std::sort(first.rbegin(),first.rend());for(size_t i=0;i<std::min<size_t>(3,first.size());i++)out.newestCountries.push_back(first[i].second);
 const auto locationSql=std::string(cache.includeQuestionable?"":" AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0))")+(cache.filtered?" AND NOT dx_location_mismatch(country,grid,call,"+std::to_string(tolerance)+")":"");
 auto recent=prepare(db_,"SELECT count(DISTINCT call) FROM decodes WHERE reception_id>? AND utc BETWEEN ? AND ? AND call!='' AND call NOT LIKE '<%' AND (coalesce(json_extract(payload,'$.source'),'legacy')!='legacy' OR utc>=?)"+locationSql);sqlite3_bind_int64(recent.get(),1,afterRowId);sqlite3_bind_int64(recent.get(),2,now-300);sqlite3_bind_int64(recent.get(),3,now+30);sqlite3_bind_int64(recent.get(),4,since);if(step(db_,recent.get())==SQLITE_ROW)out.recentStations=sqlite3_column_int64(recent.get(),0);return out;
}
void Store::expireSession(const std::vector<std::pair<int64_t,Decode>>& expired){
 expireStats(sessionCache_,expired);expireStats(historyCache_,expired);
}
void Store::expireStats(StatsCache& cache,const std::vector<std::pair<int64_t,Decode>>& expired){
 if(cache.after<0||expired.empty())return;
 try{
  Filter location;location.includeQuestionable=cache.includeQuestionable;location.removeLikelyFalse=cache.filtered;location.falseToleranceKm=cache.tolerance;
  std::set<int64_t> ids;std::set<std::string> calls,countries;for(const auto& [id,d]:expired){if(id<=cache.after||id>cache.cursor||(d.source=="legacy"&&d.utc<cache.since)||!passesLocationFilter(d,location))continue;ids.insert(id);if(!d.identity.transmitter.empty()&&d.identity.transmitter.front()!='<')calls.insert(d.identity.transmitter);if(!d.country.empty())countries.insert(d.country);}cache.totals.messages-=int64_t(ids.size());
  const std::string scope=std::string("reception_id>? AND reception_id<=? AND (coalesce(json_extract(payload,'$.source'),'legacy')!='legacy' OR utc>=?)")+(cache.includeQuestionable?"":" AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0))")+(cache.filtered?" AND NOT dx_location_mismatch(country,grid,call,"+std::to_string(cache.tolerance)+")":"");
  auto scoped=[&](const std::string& sql,const std::string* value=nullptr){auto s=prepare(db_,sql);int pos=1;if(value)sqlite3_bind_text(s.get(),pos++,value->c_str(),int(value->size()),SQLITE_TRANSIENT);sqlite3_bind_int64(s.get(),pos++,cache.after);sqlite3_bind_int64(s.get(),pos++,cache.cursor);sqlite3_bind_int64(s.get(),pos,cache.since);return s;};
  auto refresh=[&](auto& cache,const auto& values,const char* field,const char* index){for(const auto& value:values){auto found=cache.find(value);if(found==cache.end()||!ids.count(found->second.first))continue;auto s=scoped("SELECT reception_id,payload FROM decodes INDEXED BY "+std::string(index)+" WHERE "+field+"=? AND "+scope+" ORDER BY utc DESC,reception_id DESC LIMIT 1",&value);if(step(db_,s.get())==SQLITE_ROW)found->second={sqlite3_column_int64(s.get(),0),decode(s.get(),1)};else cache.erase(found);}};
  refresh(cache.calls,calls,"call","by_call_time");refresh(cache.countries,countries,"country","by_country");
  for(const auto& country:countries){auto found=cache.countryFirst.find(country);if(found==cache.countryFirst.end()||!ids.count(found->second))continue;auto s=scoped("SELECT reception_id FROM decodes INDEXED BY by_country_id WHERE country=? AND "+scope+" ORDER BY reception_id LIMIT 1",&country);if(step(db_,s.get())==SQLITE_ROW)found->second=sqlite3_column_int64(s.get(),0);else cache.countryFirst.erase(found);}
  if(ids.count(cache.farthestId)){auto s=scoped("SELECT reception_id,payload FROM decodes INDEXED BY by_distance WHERE "+scope+" AND km IS NOT NULL"+(cache.receivedOnly?" AND json_extract(payload,'$.grid_source')='received'":"")+" ORDER BY km DESC,utc DESC,reception_id DESC LIMIT 1");auto& totals=cache.totals;if(step(db_,s.get())==SQLITE_ROW){auto d=decode(s.get(),1);cache.farthestId=sqlite3_column_int64(s.get(),0);totals.farthest=d.km.value_or(0);totals.farthestCountry=d.country;totals.farthestCall=d.identity.transmitter;totals.farthestReception=std::move(d);}else{cache.farthestId=0;totals.farthest=0;totals.farthestCountry.clear();totals.farthestCall.clear();totals.farthestReception.reset();}}
 }catch(...){invalidateSession();throw;}
}
}
