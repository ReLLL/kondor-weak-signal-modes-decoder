// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include "country_summary.h"
#include <memory>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <regex>
#include <set>
#include <tuple>
#include <cmath>
#include <ctime>
#include <fnmatch.h>
namespace dx {
namespace {
constexpr int64_t hashMatchWindowSeconds=12*60;
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db,const std::string& sql){sqlite3_stmt* raw=nullptr;if(sqlite3_prepare_v2(db,sql.c_str(),-1,&raw,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));return {raw,sqlite3_finalize};}
void bindText(sqlite3_stmt* s,int pos,const std::string& v){sqlite3_bind_text(s,pos,v.c_str(),int(v.size()),SQLITE_TRANSIENT);}
std::string column(sqlite3_stmt* s,int i){const auto* p=sqlite3_column_text(s,i);return p?reinterpret_cast<const char*>(p):"";}
int step(sqlite3* db,sqlite3_stmt* s){int code=sqlite3_step(s);if(code!=SQLITE_ROW&&code!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));return code;}
struct SqlPatterns {bool all=false;std::vector<std::string> values;};
void sqlMatch(sqlite3_context* c,int,sqlite3_value** v){
 try{
  auto* patterns=static_cast<SqlPatterns*>(sqlite3_get_auxdata(c,1));std::unique_ptr<SqlPatterns> created;
  if(!patterns){created=std::make_unique<SqlPatterns>();auto raw=sqlite3_value_text(v[1]);std::string input=raw?reinterpret_cast<const char*>(raw):"";created->all=input.empty();std::stringstream in(upper(input));std::string part;
   while(std::getline(in,part,',')){auto first=part.find_first_not_of(' '),last=part.find_last_not_of(' ');if(first!=std::string::npos)created->values.push_back(part.substr(first,last-first+1));}patterns=created.get();
  }
  auto raw=sqlite3_value_text(v[0]);auto value=upper(raw?reinterpret_cast<const char*>(raw):"");bool matched=patterns->all;for(const auto& pattern:patterns->values)if(fnmatch(pattern.c_str(),value.c_str(),0)==0){matched=true;break;}sqlite3_result_int(c,matched);
  // SQLite may destroy auxiliary data immediately, so transfer ownership last.
  if(created)sqlite3_set_auxdata(c,1,created.release(),[](void* data){delete static_cast<SqlPatterns*>(data);});
 }catch(const std::bad_alloc&){sqlite3_result_error_nomem(c);}
}
void sqlBand(sqlite3_context* c,int,sqlite3_value** v){auto band=bandName(sqlite3_value_int64(v[0]));sqlite3_result_text(c,band.c_str(),-1,SQLITE_TRANSIENT);}
std::string csvField(std::string s){size_t p=0;while((p=s.find('"',p))!=std::string::npos){s.insert(p,1,'"');p+=2;}return '"'+s+'"';}
}
void Store::exec(const std::string& sql){char* error=nullptr;if(sqlite3_exec(db_,sql.c_str(),nullptr,nullptr,&error)!=SQLITE_OK){std::string message=error?error:"SQLite error";sqlite3_free(error);throw std::runtime_error(message);}}
Store::Store(const std::filesystem::path& file){
 if(sqlite3_open_v2(file.u8string().c_str(),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_NOMUTEX,nullptr)!=SQLITE_OK){std::string error=sqlite3_errmsg(db_);sqlite3_close(db_);db_=nullptr;throw std::runtime_error(error);}
 sqlite3_busy_timeout(db_,2000);sqlite3_create_function(db_,"dx_match",2,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlMatch,nullptr,nullptr);
 sqlite3_create_function(db_,"dx_band",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,sqlBand,nullptr,nullptr);
 sqlite3_create_function(db_,"dx_decode_suspect",3,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,[](sqlite3_context* c,int,sqlite3_value** v){
  try{const auto* mode=sqlite3_value_text(v[0]);if(!mode){sqlite3_result_int(c,0);return;}const std::string name(reinterpret_cast<const char*>(mode));if(name!="FT8"&&name!="FT4"){sqlite3_result_int(c,0);return;}const auto* raw=sqlite3_value_text(v[1]);sqlite3_result_int(c,sqlite3_value_int(v[2])||decoderDiagnostics(name,raw?reinterpret_cast<const char*>(raw):"").suspect);}catch(...){sqlite3_result_int(c,1);}
 },nullptr,nullptr);
 sqlite3_create_function(db_,"dx_decoder_quality",2,SQLITE_UTF8|SQLITE_DETERMINISTIC,nullptr,[](sqlite3_context* c,int,sqlite3_value** v){try{auto m=sqlite3_value_text(v[0]),r=sqlite3_value_text(v[1]);auto d=decoderDiagnostics(m?reinterpret_cast<const char*>(m):"",r?reinterpret_cast<const char*>(r):"");if(d.quality)sqlite3_result_double(c,*d.quality);else sqlite3_result_null(c);}catch(...){sqlite3_result_null(c);}},nullptr,nullptr);
 sqlite3_create_function(db_,"dx_country_distance",1,SQLITE_UTF8,this,[](sqlite3_context* c,int,sqlite3_value** v){try{auto* store=static_cast<Store*>(sqlite3_user_data(c));auto* country=sqlite3_value_text(v[0]);auto estimate=store->countryDistance(country?reinterpret_cast<const char*>(country):"");if(estimate)sqlite3_result_double(c,estimate->km);else sqlite3_result_null(c);}catch(const std::bad_alloc&){sqlite3_result_error_nomem(c);}},nullptr,nullptr);
 sqlite3_create_function(db_,"dx_location_mismatch",4,SQLITE_UTF8|SQLITE_DETERMINISTIC,this,[](sqlite3_context* c,int,sqlite3_value** v){try{auto string=[](sqlite3_value* value){const auto* s=sqlite3_value_text(value);return s?std::string(reinterpret_cast<const char*>(s)):std::string{};};auto* store=static_cast<Store*>(sqlite3_user_data(c));sqlite3_result_int(c,store->locationCheck_.mismatch(string(v[0]),string(v[1]),string(v[2]),sqlite3_value_double(v[3])));}catch(...){sqlite3_result_int(c,0);}},nullptr,nullptr);
 try{
 exec("PRAGMA auto_vacuum=INCREMENTAL;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;"
 "CREATE TABLE IF NOT EXISTS decodes(reception_id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE,utc INTEGER NOT NULL,dial INTEGER NOT NULL,mode TEXT NOT NULL,call TEXT NOT NULL,called TEXT,country TEXT,continent TEXT,grid TEXT,type TEXT,band TEXT,snr INTEGER,dt REAL,km REAL,message TEXT,payload TEXT NOT NULL);"
 "CREATE INDEX IF NOT EXISTS by_time ON decodes(utc);CREATE INDEX IF NOT EXISTS by_station ON decodes(call,dial,mode,utc);"
 "CREATE INDEX IF NOT EXISTS by_message ON decodes(call,dial,mode,message,utc);"
 "CREATE INDEX IF NOT EXISTS by_call_message ON decodes(call,message,utc);"
 "CREATE INDEX IF NOT EXISTS by_call_time ON decodes(call,utc);"
 "CREATE INDEX IF NOT EXISTS by_country ON decodes(country,utc);CREATE INDEX IF NOT EXISTS by_grid ON decodes(grid,utc);CREATE INDEX IF NOT EXISTS by_distance ON decodes(km DESC,utc DESC);"
 "CREATE TABLE IF NOT EXISTS stations(call TEXT,dial INTEGER,mode TEXT,utc INTEGER,count INTEGER,payload TEXT,PRIMARY KEY(call,dial,mode));"
 "CREATE TABLE IF NOT EXISTS ignored_transmitters(call TEXT PRIMARY KEY);"
 "CREATE TABLE IF NOT EXISTS ignored_messages(message TEXT PRIMARY KEY);"
 "CREATE TABLE IF NOT EXISTS heard(kind TEXT,value TEXT,utc INTEGER,PRIMARY KEY(kind,value));"
 "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,payload TEXT);"
 "CREATE TABLE IF NOT EXISTS imports(path TEXT PRIMARY KEY,cursor INTEGER);"
 "CREATE TABLE IF NOT EXISTS counters(key TEXT PRIMARY KEY,value INTEGER);INSERT OR IGNORE INTO counters VALUES('messages',0);");
 ensureReceptionIds();
 exec("CREATE INDEX IF NOT EXISTS by_call_received_grid ON decodes(call,utc DESC,reception_id DESC) WHERE grid!='' AND json_extract(payload,'$.grid_source')='received'");
 if(getSetting("country_names",0).get<int>()<1){exec("BEGIN IMMEDIATE");try{
  exec("UPDATE decodes SET country='Germany',payload=json_set(payload,'$.country','Germany') WHERE country='Fed. Rep. of Germany';"
   "UPDATE stations SET payload=json_set(payload,'$.country','Germany') WHERE json_extract(payload,'$.country')='Fed. Rep. of Germany';"
   "INSERT INTO heard SELECT kind,'Germany',utc FROM heard WHERE kind='country' AND value='Fed. Rep. of Germany' ON CONFLICT(kind,value) DO UPDATE SET utc=min(utc,excluded.utc);"
   "DELETE FROM heard WHERE kind='country' AND value='Fed. Rep. of Germany';");
  auto farthest=getSetting("farthest_country",Json(""));if(farthest.is_string())setSetting("farthest_country",countryName(farthest.get<std::string>()));
  setSetting("country_names",1);exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}}
 ensureEntities();
 // Rebuild derived discovery/record metadata once. Reception evidence and CSV
 // stay untouched; old questionable receptions must not suppress a new record.
 if(getSetting("confidence_schema",0).get<int>()<1){exec("BEGIN IMMEDIATE");try{
  const std::string trusted="NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0))";
  exec("DELETE FROM heard;INSERT INTO heard SELECT 'call',call,min(utc) FROM decodes WHERE call!='' AND call NOT LIKE '<%' AND "+trusted+" GROUP BY call;"
   "INSERT INTO heard SELECT 'country',country,min(utc) FROM decodes WHERE country!='' AND "+trusted+" GROUP BY country;"
   "INSERT INTO heard SELECT 'grid',grid,min(utc) FROM decodes WHERE grid!='' AND json_extract(payload,'$.grid_source')='received' AND "+trusted+" GROUP BY grid");
  auto far=prepare(db_,"SELECT km,country FROM decodes WHERE km IS NOT NULL AND json_extract(payload,'$.grid_source')='received' AND "+trusted+" ORDER BY km DESC,utc DESC,reception_id DESC LIMIT 1");bool found=step(db_,far.get())==SQLITE_ROW;setSetting("farthest_km",found?sqlite3_column_double(far.get(),0):0.);setSetting("farthest_country",found?column(far.get(),1):"");setSetting("confidence_schema",1);exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}}
 }catch(...){sqlite3_close(db_);db_=nullptr;throw;}
}
Store::~Store(){if(db_)sqlite3_close(db_);}
Json Store::getSetting(const std::string& key,const Json& fallback){auto s=prepare(db_,"SELECT payload FROM settings WHERE key=?");bindText(s.get(),1,key);if(step(db_,s.get())!=SQLITE_ROW)return fallback;auto j=Json::parse(column(s.get(),0),nullptr,false);return j.is_discarded()?fallback:j;}
void Store::setSetting(const std::string& key,const Json& value){auto s=prepare(db_,"INSERT OR REPLACE INTO settings VALUES(?,?)");bindText(s.get(),1,key);bindText(s.get(),2,value.dump());step(db_,s.get());}
std::vector<std::pair<int64_t,Decode>> Store::logRows(int64_t after,int limit){auto s=prepare(db_,"SELECT rowid,payload FROM decodes WHERE rowid>? ORDER BY rowid LIMIT ?");sqlite3_bind_int64(s.get(),1,after);sqlite3_bind_int(s.get(),2,limit);std::vector<std::pair<int64_t,Decode>> out;while(step(db_,s.get())==SQLITE_ROW)out.push_back({sqlite3_column_int64(s.get(),0),Decode::fromJson(Json::parse(column(s.get(),1)))});return out;}
int64_t Store::lastReceptionId(){auto s=prepare(db_,"SELECT max(coalesce((SELECT value FROM counters WHERE key='reception_id'),0),coalesce((SELECT max(rowid) FROM decodes),0))");return step(db_,s.get())==SQLITE_ROW?sqlite3_column_int64(s.get(),0):0;}
bool Store::ignored(const std::string& call){auto s=prepare(db_,"SELECT 1 FROM ignored_transmitters WHERE call=?");bindText(s.get(),1,upper(call));return step(db_,s.get())==SQLITE_ROW;}
std::vector<std::string> Store::ignoredCalls(){auto s=prepare(db_,"SELECT call FROM ignored_transmitters ORDER BY call");std::vector<std::string> out;while(step(db_,s.get())==SQLITE_ROW)out.push_back(column(s.get(),0));return out;}
bool Store::ignoredMessage(const std::string& message){auto s=prepare(db_,"SELECT 1 FROM ignored_messages WHERE message=?");bindText(s.get(),1,upper(message));return step(db_,s.get())==SQLITE_ROW;}
std::vector<std::string> Store::ignoredMessages(){auto s=prepare(db_,"SELECT message FROM ignored_messages ORDER BY message");std::vector<std::string> out;while(step(db_,s.get())==SQLITE_ROW)out.push_back(column(s.get(),0));return out;}
bool Store::ignoredReception(const Decode& d){return ignoredMessage(d.message)||ignored(d.identity.transmitter)||ignored(meaning(d.message).transmitter);}
void Store::unignoreAll(){exec("DELETE FROM ignored_transmitters;DELETE FROM ignored_messages");}
int64_t Store::ignoreTransmitter(const std::string& value,bool deleteRecords){return moderate({upper(value),false},deleteRecords,true);}
int64_t Store::ignoreMessage(const std::string& value,bool deleteRecords){return moderate({upper(value),true},deleteRecords,true);}
int64_t Store::deleteTransmitter(const std::string& value,bool ignoreFuture){return moderate({upper(value),false},true,ignoreFuture);}
int64_t Store::deleteMessage(const std::string& value,bool ignoreFuture){return moderate({upper(value),true},true,ignoreFuture);}
int64_t Store::moderate(const IgnoreTarget& target,bool deleteRecords,bool ignoreFuture){
 const auto& call=target.value;if(call.empty()||call.find('\0')!=std::string::npos)throw std::runtime_error("Choose a transmitter identity or message");int64_t removed=0;
 exec("BEGIN IMMEDIATE");try{
  {auto highWater=lastReceptionId();auto sequence=prepare(db_,"INSERT OR REPLACE INTO counters VALUES('reception_id',?)");sqlite3_bind_int64(sequence.get(),1,highWater);step(db_,sequence.get());
   if(ignoreFuture){auto ignore=prepare(db_,target.message?"INSERT OR IGNORE INTO ignored_messages VALUES(?)":"INSERT OR IGNORE INTO ignored_transmitters VALUES(?)");bindText(ignore.get(),1,call);step(db_,ignore.get());}
   if(deleteRecords){auto remove=prepare(db_,target.message?"DELETE FROM decodes WHERE upper(message)=?":"DELETE FROM decodes WHERE call=?");bindText(remove.get(),1,call);step(db_,remove.get());removed=sqlite3_changes(db_);
    if(!target.message){auto station=prepare(db_,"DELETE FROM stations WHERE call=?");bindText(station.get(),1,call);step(db_,station.get());}rebuildSummaries(target.message);if(removed)setSetting("compact_pending",true);
    const char* purgeKey=target.message?"message_log_purges":"log_purges";auto purges=getSetting(purgeKey,Json::object());purges[call]=std::max(purges.value(call,int64_t(0)),highWater);setSetting(purgeKey,purges);
   }
  }exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
 return removed;
}
bool Store::insert(Decode& d,Point home,const std::function<std::pair<std::string,std::string>(const std::string&)>& country){
 // An old legacy file must never repopulate history that retention expired.
 if(d.source=="legacy"&&d.utc<=getSetting("retention_floor",int64_t(0)).get<int64_t>())return false;
 if(ignoredMessage(d.message))return false;d.countryDistanceGrid.clear();d.identity=decodedMeaning(d.mode,d.message,d.raw);auto diagnostics=decoderDiagnostics(d.mode,d.raw);diagnostics.suspect|=d.diagnostics.suspect;if(!diagnostics.quality)diagnostics.quality=d.diagnostics.quality;if(diagnostics.flags.empty())diagnostics.flags=d.diagnostics.flags;d.diagnostics=std::move(diagnostics);auto& i=d.identity;if(ignored(i.transmitter))return false;
 auto resolve=[&](std::string& value){std::smatch match;static const std::regex hash("<([0-2]):([0-9]{1,7})>");if(!std::regex_match(value,match,hash))return;int bits=match[1]=="0"?10:match[1]=="1"?12:22;uint32_t wanted=uint32_t(std::stoul(match[2]));
  auto s=prepare(db_,"SELECT DISTINCT call FROM decodes WHERE utc BETWEEN ? AND ? AND call!='' AND call NOT LIKE '<%' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0))");sqlite3_bind_int64(s.get(),1,d.utc-hashMatchWindowSeconds);sqlite3_bind_int64(s.get(),2,d.utc);std::string candidate;bool ambiguous=false;
  while(step(db_,s.get())==SQLITE_ROW){auto call=column(s.get(),0);if(callHash(call,bits)==wanted){if(!candidate.empty()&&candidate!=call)ambiguous=true;candidate=call;}}
  if(!ambiguous&&!candidate.empty()){value=candidate;d.identitySource="unique-recent-hash";}
 };resolve(i.transmitter);resolve(i.called);
 if(ignored(i.transmitter))return false;
 if(country&&!i.transmitter.empty()&&i.transmitter.front()!='<'){auto c=country(i.transmitter);d.country=c.first;d.continent=c.second;}
 d.country=countryName(d.country);
 if(!i.grid.empty()){d.gridSource="received";d.gridUtc=d.utc;d.position=gridPoint(i.grid);}
 else if(!i.transmitter.empty()&&i.transmitter.front()!='<'){
  auto s=prepare(db_,"SELECT payload FROM decodes INDEXED BY by_call_received_grid WHERE call=? AND utc<=? AND utc>=? AND grid!='' AND json_extract(payload,'$.grid_source')='received' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0)) ORDER BY utc DESC LIMIT 1");bindText(s.get(),1,i.transmitter);sqlite3_bind_int64(s.get(),2,d.utc);sqlite3_bind_int64(s.get(),3,d.utc-locationCacheSeconds);
  if(step(db_,s.get())==SQLITE_ROW){auto last=Decode::fromJson(Json::parse(column(s.get(),0)));if(last.gridSource=="received"){i.grid=last.identity.grid;d.gridSource="cached";d.gridUtc=last.utc;d.position=gridPoint(i.grid);}}
 }
 d.km.reset();d.bearing.reset();if(d.position&&std::isfinite(home.lat)&&std::isfinite(home.lon)){auto b=distanceBearing(home,*d.position);d.km=b.first;d.bearing=b.second;}
 auto key=d.key();auto duplicate=prepare(db_,"SELECT 1 FROM decodes WHERE key=?");bindText(duplicate.get(),1,key);if(step(db_,duplicate.get())==SQLITE_ROW)return false;
 exec("BEGIN IMMEDIATE");try{
  auto heard=[&](const std::string& kind,const std::string& value){if(d.diagnostics.suspect||value.empty()||value.front()=='<')return false;auto s=prepare(db_,"INSERT OR IGNORE INTO heard VALUES(?,?,?)");bindText(s.get(),1,kind);bindText(s.get(),2,value);sqlite3_bind_int64(s.get(),3,d.utc);step(db_,s.get());return sqlite3_changes(db_)==1;};
  d.firstCall=heard("call",i.transmitter);d.firstGrid=d.gridSource=="received"&&heard("grid",i.grid);d.firstCountry=heard("country",d.country);
  d.distanceRecord=false;if(!d.diagnostics.suspect&&d.km&&d.gridSource=="received"&&*d.km>getSetting("farthest_km",0.).get<double>()){d.distanceRecord=true;setSetting("farthest_km",*d.km);setSetting("farthest_country",d.country);}
  auto payload=d.json().dump();auto s=prepare(db_,"INSERT INTO decodes(rowid,key,utc,dial,mode,call,called,country,continent,grid,type,band,snr,dt,km,message,payload) VALUES((SELECT max(coalesce((SELECT value FROM counters WHERE key='reception_id'),0),coalesce((SELECT max(rowid) FROM decodes),0))+1),?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  bindText(s.get(),1,key);sqlite3_bind_int64(s.get(),2,d.utc);sqlite3_bind_int64(s.get(),3,d.dial);bindText(s.get(),4,d.mode);bindText(s.get(),5,i.transmitter);bindText(s.get(),6,i.called);bindText(s.get(),7,d.country);bindText(s.get(),8,d.continent);bindText(s.get(),9,i.grid);bindText(s.get(),10,i.type);bindText(s.get(),11,bandName(d.dial));sqlite3_bind_int(s.get(),12,d.snr);if(d.dt)sqlite3_bind_double(s.get(),13,*d.dt);if(d.km)sqlite3_bind_double(s.get(),14,*d.km);bindText(s.get(),15,d.message);bindText(s.get(),16,payload);step(db_,s.get());
  exec("INSERT OR REPLACE INTO counters VALUES('reception_id',last_insert_rowid())");
  if(!i.transmitter.empty()){
   auto a=prepare(db_,"INSERT INTO stations VALUES(?,?,?,?,1,?) ON CONFLICT(call,dial,mode) DO UPDATE SET count=count+1,payload=CASE WHEN excluded.utc>=utc THEN excluded.payload ELSE payload END,utc=max(utc,excluded.utc)");bindText(a.get(),1,i.transmitter);sqlite3_bind_int64(a.get(),2,d.dial);bindText(a.get(),3,d.mode);sqlite3_bind_int64(a.get(),4,d.utc);bindText(a.get(),5,payload);step(db_,a.get());
  }
  if(!d.diagnostics.suspect&&d.km&&d.gridSource=="received"){
   auto previous=getSetting("farthest_km",0.).get<double>();if(*d.km>previous)setSetting("farthest_km",*d.km);
  }
  exec("UPDATE counters SET value=value+1 WHERE key='messages';COMMIT");return true;
 }catch(...){exec("ROLLBACK");throw;}
}
std::vector<Row> Store::rows(const Query& q,int offset,int limit,int64_t* countOnly){
 const auto& f=q.filter;
 Filter all;all.includeQuestionable=true;bool empty=f.includeQuestionable&&f.json()==all.json();std::vector<std::string> values;
 std::string where="utc>=? AND utc<=?";
 // Station-mode queries match the stations cache: unattributed text is kept
 // in reception/unique-sender tables, but is not a station/map entry.
 if(!q.messages&&!q.uniqueCalls)where+=" AND call!=''";
 if(q.afterRowId>0)where+=" AND rowid>?";
 auto match=[&](const std::string& field,const std::string& patterns,bool exclude=false){
  if(patterns.empty())return;
  if(field=="country"){
   // Expand against the small country summary, then let by_country select records.
   // Wildcards/comma lists retain exactly the same matcher as other filters.
   std::vector<std::string> selected;auto countries=prepare(db_,"SELECT value FROM entities WHERE kind='country'");while(step(db_,countries.get())==SQLITE_ROW){auto country=column(countries.get(),0);if(matches(country,patterns))selected.push_back(country);}
   bool unknown=matches("",patterns);if(unknown)selected.push_back("");
   if(selected.empty()){where+=exclude?" AND 1":" AND 0";return;}
   std::string placeholders;for(const auto& country:selected){if(!placeholders.empty())placeholders+=',';placeholders+='?';values.push_back(country);}
   where+=exclude?" AND (country IS "+std::string(unknown?"NOT NULL AND ":"NULL OR ")+"country NOT IN ("+placeholders+"))":" AND (country IN ("+placeholders+")"+(unknown?" OR country IS NULL":"")+")";return;
  }
  auto first=patterns.find_first_not_of(' '),last=patterns.find_last_not_of(' ');
  if(first!=std::string::npos&&patterns.find_first_of(",*?[]\\")==std::string::npos){auto value=patterns.substr(first,last-first+1);bool uppercase=field=="call"||field=="grid"||field=="mode";if(uppercase)value=upper(value);std::string compare=field+(exclude?"!=?":"=?")+(uppercase?"":" COLLATE NOCASE");where+=exclude?" AND ("+field+" IS NULL OR "+compare+")":" AND "+compare;values.push_back(std::move(value));return;}
  where+=(exclude?" AND NOT dx_match(":" AND dx_match(")+field+",?)";values.push_back(patterns);
 };
 const std::string target="CASE WHEN called IN ('CQ','QRZ','DE') THEN '' ELSE called END";match("json_extract(payload,'$.source')",f.source);match("call",f.call);match(target,f.called);match("country",f.country);match("continent",f.continent);match("grid",f.grid);match("band",f.band);match("mode",f.mode);match("CAST(dial AS TEXT)",f.frequency);match("type",f.type);
 match("call",f.excludeCall,true);match(target,f.excludeCalled,true);match("country",f.excludeCountry,true);match("grid",f.excludeGrid,true);
 if(!f.prefix.empty()){std::stringstream in(f.prefix);std::string p,patterns;while(std::getline(in,p,',')){if(!patterns.empty())patterns+=',';patterns+=p+'*';}match("call",patterns);}
 if(!f.search.empty()){where+=" AND instr(upper(message||' '||country||' '||call),?)>0";values.push_back(upper(f.search));}
 if(!f.text.empty()){where+=" AND instr(upper(message),?)>0";values.push_back(upper(f.text));}
 if(f.cqOnly)where+=" AND type='CQ'";if(f.hasGrid)where+=" AND grid!=''";if(f.hasTarget)where+=" AND called NOT IN ('','CQ','QRZ','DE')";if(f.first)where+=" AND json_extract(payload,'$.first_call')=1";
 if(!f.includeQuestionable)where+=" AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0))";
 if(f.removeLikelyFalse)where+=" AND NOT dx_location_mismatch(country,grid,call,"+std::to_string(std::isfinite(f.falseToleranceKm)?std::clamp(f.falseToleranceKm,0.,2000.):300.)+")";
 if(f.messageKind==MessageKind::GeneralCall)where+=" AND called IN ('CQ','QRZ')";
 if(f.messageKind==MessageKind::Exchange)where+=" AND call!='' AND called NOT IN ('','CQ','QRZ','DE') AND called NOT LIKE '@%' AND type!='Heartbeat'";
 if(f.messageKind==MessageKind::Other)where+=" AND type NOT IN ('Beacon','Heartbeat') AND called NOT IN ('CQ','QRZ') AND (call='' OR called IN ('','DE') OR called LIKE '@%')";
 if(f.messageKind==MessageKind::Beacon)where+=" AND type IN ('Beacon','Heartbeat')";
 where+=" AND snr>="+std::to_string(f.minSnr);
 if(f.maxDt<30)where+=" AND abs(dt)<="+std::to_string(f.maxDt);
 const auto distance=distanceSql();if(f.minKm>0||f.maxKm<40080)where+=" AND "+distance+" BETWEEN "+std::to_string(f.minKm)+" AND "+std::to_string(f.maxKm);
 const bool cached=q.since==0&&q.until==INT64_MAX&&q.afterRowId==0&&empty&&!q.messages&&!q.uniqueCalls;
 const bool entityCached=q.since==0&&q.until==INT64_MAX&&q.afterRowId==0&&empty&&!q.messages&&q.uniqueCalls;
 if(countOnly&&empty&&q.messages&&q.since==0&&q.until==INT64_MAX&&q.afterRowId==0){*countOnly=savedMessageCount();return {};}
 const char* fields[]={"timestamp","transmitter","country","km","snr","grid","mode","frequency","frequency","audio_hz","dt","bearing","","called","type","message","","","timestamp","source","decoder_quality"};
 int sort=std::clamp(q.sort,0,20);std::string order=sort==17?"station_first_heard":sort==16?"first_heard":sort==12?"row_count":"json_extract(payload,'$."+std::string(fields[sort])+"')";
 const char* indexed[]={"utc","call","country","km","snr","grid","mode","band","dial","","dt","","","called","type","message","","","utc",""};
 if(sort<20&&*indexed[sort]&&(!cached||sort==0||sort==1||sort==6||sort==8||sort==18))order=indexed[sort];
 if(sort==7)order="dx_band(dial)";if(sort==13)order="CASE WHEN json_extract(payload,'$.called') IN ('','CQ','QRZ','DE') THEN NULL ELSE json_extract(payload,'$.called') END";
 if(sort==3)order=distanceSql(cached);if(sort==20)order="coalesce(json_extract(payload,'$.decoder_quality'),dx_decoder_quality(mode,json_extract(payload,'$.raw')))";
 std::string sql;
 const std::string group=q.uniqueCalls?"(call=''),CASE WHEN call='' THEN message ELSE call END":"CASE WHEN call='' THEN key ELSE call END,dial,mode";
 if(entityCached)sql="SELECT d.*,e.total AS row_count FROM entities AS e JOIN decodes AS d ON d.reception_id=e.last_id WHERE e.kind IN ('call','unknown')";
 else if(cached)sql="SELECT *,count AS row_count FROM stations";
 else if(q.messages)sql="SELECT rowid AS reception_order,*,1 AS row_count FROM decodes WHERE "+where;
 else if(countOnly&&!f.hasColumnRanges())sql="SELECT 1 FROM decodes WHERE "+where+" GROUP BY "+group;
 else {
  // FT8/FT4 receptions can share a slot timestamp. The last inserted decode
  // wins that tie, keeping message text consistent with the station cache.
  // Rank compact row IDs before fetching JSON, rather than sorting every payload.
  sql="SELECT d.*,latest.row_count FROM decodes AS d JOIN (SELECT rowid AS reception_id,count(*) OVER (PARTITION BY "+group+") AS row_count,row_number() OVER (PARTITION BY "+group+" ORDER BY utc DESC,rowid DESC) AS latest_row FROM decodes WHERE "+where+") AS latest ON d.rowid=latest.reception_id WHERE latest.latest_row=1";
 }
 if(countOnly&&!f.hasColumnRanges())sql="SELECT count(*) FROM ("+sql+")";
 else {
  // Session/clear cutoffs are independent of rolling time and display filters.
  auto cutoff=" AND first.utc>="+std::to_string(q.firstSince)+" AND first.rowid>"+std::to_string(q.firstAfterId)+(f.includeQuestionable?"":" AND NOT dx_decode_suspect(first.mode,json_extract(first.payload,'$.raw'),coalesce(json_extract(first.payload,'$.decoder_suspect'),0))");
  auto first="coalesce((SELECT min(first.utc) FROM decodes AS first INDEXED BY by_call_message WHERE first.call=shown.call AND first.message=json_extract(shown.payload,'$.message')"+cutoff+"),shown.utc)";
  auto station="(SELECT min(first.utc) FROM decodes AS first INDEXED BY by_call_time WHERE first.call=shown.call AND shown.call!='' AND shown.call NOT LIKE '<%'"+cutoff+")";
  if(f.hasColumnRanges()){
   auto reference=std::to_string(q.referenceUtc?q.referenceUtc:int64_t(time(nullptr)));std::array<std::string,11> metrics={"snr",distance,"dial/1000000.0","json_extract(payload,'$.audio_hz')","dt","json_extract(payload,'$.bearing')","row_count","max(0,"+reference+"-utc)","max(0,"+reference+"-first_heard)","max(0,"+reference+"-station_first_heard)","coalesce(json_extract(payload,'$.decoder_quality'),dx_decoder_quality(mode,json_extract(payload,'$.raw')))"};
   auto number=[](double value){std::ostringstream out;out<<std::setprecision(17)<<value;return out.str();};std::string conditions="1";for(size_t i=0;i<metrics.size();i++){const auto& r=f.columnRanges[i];if(r.min)conditions+=std::isfinite(*r.min)?" AND ("+metrics[i]+")>="+number(*r.min):" AND 0";if(r.max)conditions+=std::isfinite(*r.max)?" AND ("+metrics[i]+")<="+number(*r.max):" AND 0";}
   sql="WITH shown AS ("+sql+"),enriched AS (SELECT shown.*,"+first+" AS first_heard,"+station+" AS station_first_heard FROM shown) SELECT "+(countOnly?"count(*)":"payload,row_count,first_heard,station_first_heard")+" FROM enriched WHERE "+conditions;
   if(!countOnly)sql+=" ORDER BY ("+order+" IS NULL) ASC,"+order+(q.descending?" DESC":" ASC")+",utc DESC,"+(q.messages?"reception_order DESC,":"")+"call ASC,dial ASC,mode ASC LIMIT ? OFFSET ?";
  }else {
   // utc is NOT NULL: a synthetic NULL sort key prevents SQLite using by_time.
   auto nullOrder=(order=="utc"?"":"("+order+" IS NULL) ASC,");
   auto tie=q.messages&&order=="utc"?"reception_order" : "utc";
   sql="WITH shown AS ("+sql+") SELECT payload,row_count,"+first+" AS first_heard,"+station+" AS station_first_heard FROM shown ORDER BY "+nullOrder+order+(q.descending?" DESC":" ASC")+","+tie+" DESC"+(q.messages&&order!="utc"?",reception_order DESC":"")+(q.messages&&order=="utc"?"":",call ASC,dial ASC,mode ASC")+" LIMIT ? OFFSET ?";
  }
 }
 auto s=prepare(db_,sql);int pos=1;
 if(!cached&&!entityCached){sqlite3_bind_int64(s.get(),pos++,q.since);sqlite3_bind_int64(s.get(),pos++,q.until);if(q.afterRowId>0)sqlite3_bind_int64(s.get(),pos++,q.afterRowId);for(const auto& v:values)bindText(s.get(),pos++,v);}
 if(countOnly){*countOnly=step(db_,s.get())==SQLITE_ROW?sqlite3_column_int64(s.get(),0):0;return {};}
 sqlite3_bind_int(s.get(),pos++,limit);sqlite3_bind_int(s.get(),pos,offset);std::vector<Row> rows;
 while(step(db_,s.get())==SQLITE_ROW){auto d=Decode::fromJson(Json::parse(column(s.get(),0)));projectDistance(d);rows.push_back({std::move(d),sqlite3_column_int64(s.get(),1),sqlite3_column_int64(s.get(),2),sqlite3_column_int64(s.get(),3)});}return rows;
}
void Store::mapLocations(std::vector<Row>& rows){
 // Display evidence only: never rewrite the reception, its distance or logs.
 // The partial index avoids scanning a station's repeated report messages.
 auto s=prepare(db_,"SELECT grid,utc FROM decodes INDEXED BY by_call_received_grid WHERE call=? AND utc<=? AND grid!='' AND json_extract(payload,'$.grid_source')='received' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0)) ORDER BY utc DESC,reception_id DESC LIMIT 1");
 for(auto& row:rows){row.lastKnownLocation.reset();const auto& d=row.decode;if(d.position||d.identity.transmitter.empty()||d.identity.transmitter.front()=='<')continue;
  sqlite3_reset(s.get());sqlite3_clear_bindings(s.get());bindText(s.get(),1,d.identity.transmitter);sqlite3_bind_int64(s.get(),2,d.utc);
  if(step(db_,s.get())==SQLITE_ROW){auto grid=column(s.get(),0);if(auto point=gridPoint(grid))row.lastKnownLocation=MapLocation{*point,grid,sqlite3_column_int64(s.get(),1)};}
 }
}
std::vector<Decode> Store::history(const std::string& call,int64_t dial,const std::string& mode,int offset){auto s=prepare(db_,"SELECT payload FROM decodes WHERE call=? AND dial=? AND mode=? ORDER BY utc DESC LIMIT 500 OFFSET ?");bindText(s.get(),1,call);sqlite3_bind_int64(s.get(),2,dial);bindText(s.get(),3,mode);sqlite3_bind_int(s.get(),4,offset);std::vector<Decode> out;while(step(db_,s.get())==SQLITE_ROW)out.push_back(Decode::fromJson(Json::parse(column(s.get(),0))));return out;}
std::vector<Decode> Store::uniqueMessages(const Decode& station,const Query& view){
 auto s=prepare(db_,"SELECT payload FROM (SELECT payload,utc,rowid AS reception_order,row_number() OVER (PARTITION BY message ORDER BY utc DESC,rowid DESC) AS rank FROM decodes WHERE call=? AND dial=? AND mode=? AND utc BETWEEN ? AND ? AND rowid>?) WHERE rank=1 ORDER BY utc DESC,reception_order DESC LIMIT 5");
 bindText(s.get(),1,station.identity.transmitter);sqlite3_bind_int64(s.get(),2,station.dial);bindText(s.get(),3,station.mode);sqlite3_bind_int64(s.get(),4,view.since);sqlite3_bind_int64(s.get(),5,view.until);sqlite3_bind_int64(s.get(),6,view.afterRowId);
 std::vector<Decode> out;while(step(db_,s.get())==SQLITE_ROW)out.push_back(Decode::fromJson(Json::parse(column(s.get(),0))));return out;
}
int64_t Store::importBridge(const std::filesystem::path& path,Point home,const std::function<std::pair<std::string,std::string>(const std::string&)>& country){
 if(!std::filesystem::exists(path))return 0;sqlite3* source=nullptr;int code=sqlite3_open_v2(path.u8string().c_str(),&source,SQLITE_OPEN_READONLY,nullptr);std::unique_ptr<sqlite3,decltype(&sqlite3_close)> owner(source,sqlite3_close);if(code!=SQLITE_OK)throw std::runtime_error("Legacy history unavailable");
 sqlite3_busy_timeout(source,2000);auto cursor=getSetting("import_bridge_cursor",0).get<int64_t>();auto s=prepare(source,"SELECT rowid,payload FROM decodes WHERE rowid>? ORDER BY rowid LIMIT 500");sqlite3_bind_int64(s.get(),1,cursor);int64_t added=0;
 while(step(source,s.get())==SQLITE_ROW){auto j=Json::parse(column(s.get(),1));j["source"]="legacy";auto d=Decode::fromJson(j);if(insert(d,home,country))++added;cursor=sqlite3_column_int64(s.get(),0);}
 setSetting("import_bridge_cursor",cursor);return added;
}
void Store::exportRows(const Query& q,const std::filesystem::path& path,bool csv){if(q.countries){auto latest=q;latest.countries=false;latest.uniqueCalls=true;latest.messages=false;latest.sort=0;auto summaries=countrySummaries(rows(latest),q.sort,q.descending);std::ofstream out(path);if(!out)throw std::runtime_error("Export file unavailable");Json data=Json::array();if(csv)out<<"Country,Stations,AverageKm,AverageSNRdB,Last5Senders,LastHeardUnixUTC,LocatedSenders,LocalDateTime,CountryEstimatedSenders\n";for(const auto& country:summaries){if(csv)out<<csvField(country.country)<<','<<country.stations<<','<<(country.averageKm?std::to_string(*country.averageKm):"")<<','<<country.averageSnr<<','<<csvField(senderList(country))<<','<<country.lastUtc<<','<<(country.distanceSamples-country.countryEstimates)<<','<<csvField(localDateTime(country.lastUtc))<<','<<country.countryEstimates<<'\n';else{Json senders=Json::array();for(const auto& sender:country.latestSenders)senders.push_back(sender.identity.transmitter);data.push_back({{"country",country.country},{"stations",country.stations},{"average_km",country.averageKm?Json(*country.averageKm):Json(nullptr)},{"average_snr_db",country.averageSnr},{"last_5_senders",senders},{"last_heard_utc",country.lastUtc},{"located_senders",country.distanceSamples-country.countryEstimates},{"country_estimated_senders",country.countryEstimates}});}}if(!csv)out<<data.dump(2)<<'\n';if(!out)throw std::runtime_error("Export write failed");return;}
std::ofstream file(path);if(!file)throw std::runtime_error("Export file unavailable");if(csv)file<<"UTC,Call,Called,Country,Grid,Mode,DialHz,AudioHz,SNRdB,DT,ApproxKm,Message,LocalDateTime,DistanceSource,CountryEstimateGrid\n";else file<<"[\n";
 int offset=0;bool comma=false;for(;;){auto page=rows(q,offset,1000);if(page.empty())break;for(auto& row:page){auto& d=row.decode;if(csv)file<<d.utc<<','<<csvField(d.identity.transmitter)<<','<<csvField(d.identity.called)<<','<<csvField(d.country)<<','<<csvField(d.identity.grid)<<','<<d.mode<<','<<d.dial<<','<<d.audio<<','<<d.snr<<','<<(d.dt?std::to_string(*d.dt):"")<<','<<(d.km?std::to_string(*d.km):"")<<','<<csvField(d.message)<<','<<csvField(localDateTime(d.utc))<<','<<csvField(!d.countryDistanceGrid.empty()?"country":d.gridSource)<<','<<csvField(d.countryDistanceGrid)<<'\n';else{if(comma)file<<",\n";file<<d.json().dump();comma=true;}}offset+=int(page.size());}if(!csv)file<<"\n]\n";if(!file)throw std::runtime_error("Export write failed");}
void Store::rebaseDistances(Point home){
 auto origin=std::isfinite(home.lat)&&std::isfinite(home.lon)?Json::array({home.lat,home.lon}):Json(nullptr);if(getSetting("distance_origin",Json(nullptr))==origin)return;
 invalidateSession();exec("BEGIN IMMEDIATE");try{
  auto read=prepare(db_,"SELECT rowid,payload FROM decodes");auto write=prepare(db_,"UPDATE decodes SET km=?,payload=? WHERE rowid=?");
  double farthest=0;std::string country;
  while(step(db_,read.get())==SQLITE_ROW){auto d=Decode::fromJson(Json::parse(column(read.get(),1)));d.km.reset();d.bearing.reset();if(d.position&&std::isfinite(home.lat)&&std::isfinite(home.lon)){auto path=distanceBearing(home,*d.position);d.km=path.first;d.bearing=path.second;}
   if(!d.diagnostics.suspect&&d.km&&d.gridSource=="received"&&*d.km>farthest){farthest=*d.km;country=d.country;}
   sqlite3_reset(write.get());sqlite3_clear_bindings(write.get());if(d.km)sqlite3_bind_double(write.get(),1,*d.km);else sqlite3_bind_null(write.get(),1);bindText(write.get(),2,d.json().dump());sqlite3_bind_int64(write.get(),3,sqlite3_column_int64(read.get(),0));step(db_,write.get());
  }
  exec("UPDATE stations SET payload=(SELECT payload FROM decodes WHERE decodes.call=stations.call AND decodes.dial=stations.dial AND decodes.mode=stations.mode ORDER BY utc DESC,rowid DESC LIMIT 1)");
  setSetting("farthest_km",farthest);setSetting("farthest_country",country);setSetting("distance_origin",origin);exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
}
void Store::reclassifyCountries(const std::function<std::pair<std::string,std::string>(const std::string&)>& country){
 if(getSetting("country_schema",0).get<int>()>=3)return;invalidateSession();exec("BEGIN IMMEDIATE");try{
  exec("DELETE FROM heard WHERE kind='country'");auto read=prepare(db_,"SELECT key,payload FROM decodes ORDER BY utc");auto write=prepare(db_,"UPDATE decodes SET country=?,continent=?,payload=? WHERE key=?");
  while(step(db_,read.get())==SQLITE_ROW){auto d=Decode::fromJson(Json::parse(column(read.get(),1)));auto result=country(d.identity.transmitter);d.country=countryName(result.first);d.continent=result.second;d.firstCountry=false;
   if(!d.country.empty()){auto h=prepare(db_,"INSERT OR IGNORE INTO heard VALUES('country',?,?)");bindText(h.get(),1,d.country);sqlite3_bind_int64(h.get(),2,d.utc);step(db_,h.get());d.firstCountry=sqlite3_changes(db_)==1;}
   sqlite3_reset(write.get());sqlite3_clear_bindings(write.get());bindText(write.get(),1,d.country);bindText(write.get(),2,d.continent);bindText(write.get(),3,d.json().dump());bindText(write.get(),4,column(read.get(),0));step(db_,write.get());
  }
  exec("UPDATE stations SET payload=(SELECT payload FROM decodes WHERE decodes.call=stations.call AND decodes.dial=stations.dial AND decodes.mode=stations.mode ORDER BY utc DESC LIMIT 1)");setSetting("country_schema",3);exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
}
std::vector<ExchangePath> Store::exchangePaths(const Query& query){
 if(query.pathCount<=0||query.filter.messageKind==MessageKind::GeneralCall||query.filter.messageKind==MessageKind::Other)return {};
 auto q=query;q.messages=true;q.sort=0;q.descending=true;q.filter.messageKind=MessageKind::Exchange;auto recent=rows(q,0,1000);std::vector<ExchangePath> paths;
 auto last=prepare(db_,"SELECT coalesce(max(utc),0) FROM decodes WHERE utc BETWEEN ? AND ?");sqlite3_bind_int64(last.get(),1,query.since);sqlite3_bind_int64(last.get(),2,query.until);step(db_,last.get());auto end=sqlite3_column_int64(last.get(),0);
 std::set<std::tuple<std::string,std::string,int64_t,std::string>> seen;
 for(const auto& row:recent){auto& d=row.decode;if(query.pathSpectrum&&!frequencyVisible(d.dial+d.audio,*query.pathSpectrum))continue;if(d.identity.transmitter.empty()||d.identity.called.empty()||d.identity.called=="CQ"||d.identity.called.front()=='<'||d.identity.transmitter.front()=='<')continue;
  if(d.identity.transmitter==d.identity.called)continue;auto pair=std::minmax(d.identity.transmitter,d.identity.called);auto key=std::make_tuple(pair.first,pair.second,d.dial,d.mode);if(seen.count(key))continue;
  auto location=[&](const std::string& call)->std::optional<Decode>{auto s=prepare(db_,"SELECT payload FROM decodes INDEXED BY by_call_received_grid WHERE call=? AND utc BETWEEN ? AND ? AND grid!='' AND json_extract(payload,'$.grid_source')='received' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0)) ORDER BY utc DESC LIMIT 1");bindText(s.get(),1,call);sqlite3_bind_int64(s.get(),2,end-locationCacheSeconds);sqlite3_bind_int64(s.get(),3,end);if(step(db_,s.get())!=SQLITE_ROW)return {};return Decode::fromJson(Json::parse(column(s.get(),0)));};
  auto from=location(d.identity.transmitter),to=location(d.identity.called);if(!from||!to||!from->position||!to->position)continue;
  seen.insert(key);paths.push_back({d.identity.transmitter,d.identity.called,d.utc,from->utc,to->utc,greatCircle(*from->position,*to->position),d.dial+d.audio});if(int(paths.size())>=query.pathCount)break;
 }return paths;
}
}
