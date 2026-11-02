// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include <memory>
#include <set>
#include <tuple>
#include <stdexcept>
namespace dx {
namespace {
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db,const std::string& sql){sqlite3_stmt* s=nullptr;if(sqlite3_prepare_v2(db,sql.c_str(),-1,&s,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));return {s,sqlite3_finalize};}
int step(sqlite3* db,sqlite3_stmt* s){int rc=sqlite3_step(s);if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db));return rc;}
std::string text(sqlite3_stmt* s,int i){auto p=sqlite3_column_text(s,i);return p?reinterpret_cast<const char*>(p):"";}
void bind(sqlite3_stmt* s,int i,const std::string& v){sqlite3_bind_text(s,i,v.c_str(),int(v.size()),SQLITE_TRANSIENT);}
}
void Store::ensureEntities(){
 auto version=getSetting("entity_schema",0).get<int>();if(version>=2)return;
 exec("BEGIN IMMEDIATE");try{
  exec("CREATE TABLE IF NOT EXISTS entities(kind TEXT,value TEXT,total INTEGER NOT NULL,first_utc INTEGER NOT NULL,first_id INTEGER NOT NULL,last_utc INTEGER NOT NULL,last_id INTEGER NOT NULL,PRIMARY KEY(kind,value));"
       "CREATE INDEX IF NOT EXISTS by_call_id ON decodes(call,reception_id);CREATE INDEX IF NOT EXISTS by_country_id ON decodes(country,reception_id);"
       "CREATE INDEX IF NOT EXISTS by_unknown_message ON decodes(message,utc) WHERE call='';CREATE INDEX IF NOT EXISTS by_unknown_id ON decodes(message,reception_id) WHERE call='';");
  if(!version)exec("DELETE FROM entities");
  for(const auto& kind:{"call","country","unknown"}){
   std::string field=std::string(kind)=="unknown"?"message":kind;
   std::string valid=std::string(kind)=="unknown"?"call=''":field+"!=''";
   std::string timeIndex=std::string(kind)=="unknown"?"by_unknown_message":std::string(kind)=="country"?"by_country":"by_call_time";std::string idIndex=std::string(kind)=="unknown"?"by_unknown_id":std::string(kind)=="country"?"by_country_id":"by_call_id";
   auto equal=[&](const std::string& row){return field+"="+row+"."+field+(std::string(kind)=="unknown"?" AND call=''":"");};
   std::string add="INSERT INTO entities SELECT '"+std::string(kind)+"',NEW."+field+",1,NEW.utc,NEW.reception_id,NEW.utc,NEW.reception_id WHERE "+(std::string(kind)=="unknown"?"NEW.call=''":"NEW."+field+"!=''")+" ON CONFLICT(kind,value) DO UPDATE SET total=total+1,first_utc=min(first_utc,excluded.first_utc),first_id=min(first_id,excluded.first_id),last_id=CASE WHEN (excluded.last_utc,excluded.last_id)>(last_utc,last_id) THEN excluded.last_id ELSE last_id END,last_utc=max(last_utc,excluded.last_utc);";
   // Pin covering indexes: otherwise SQLite can choose the ID index for min(utc),
   // fetching an entire country's payload pages after every oldest-row deletion.
   std::string remove="UPDATE entities SET total=total-1,first_utc=CASE WHEN first_utc=OLD.utc THEN coalesce((SELECT min(utc) FROM decodes INDEXED BY "+timeIndex+" WHERE "+equal("OLD")+"),first_utc) ELSE first_utc END,first_id=CASE WHEN first_id=OLD.reception_id THEN coalesce((SELECT min(reception_id) FROM decodes INDEXED BY "+idIndex+" WHERE "+equal("OLD")+"),first_id) ELSE first_id END,last_utc=CASE WHEN last_id=OLD.reception_id THEN coalesce((SELECT utc FROM decodes INDEXED BY "+timeIndex+" WHERE "+equal("OLD")+" ORDER BY utc DESC,reception_id DESC LIMIT 1),last_utc) ELSE last_utc END,last_id=CASE WHEN last_id=OLD.reception_id THEN coalesce((SELECT reception_id FROM decodes INDEXED BY "+timeIndex+" WHERE "+equal("OLD")+" ORDER BY utc DESC,reception_id DESC LIMIT 1),last_id) ELSE last_id END WHERE kind='"+kind+"' AND value=OLD."+field+";DELETE FROM entities WHERE kind='"+kind+"' AND value=OLD."+field+" AND total=0;";
   if(!version)exec("INSERT INTO entities SELECT '"+std::string(kind)+"',"+field+",count(*),min(utc),min(reception_id),max(utc),(SELECT reception_id FROM decodes AS latest INDEXED BY "+timeIndex+" WHERE latest."+field+"=d."+field+(std::string(kind)=="unknown"?" AND latest.call=''":"")+" ORDER BY utc DESC,reception_id DESC LIMIT 1) FROM decodes AS d INDEXED BY "+timeIndex+" WHERE "+valid+" GROUP BY "+field+";");
   exec("DROP TRIGGER IF EXISTS entity_"+std::string(kind)+"_insert;DROP TRIGGER IF EXISTS entity_"+kind+"_delete;DROP TRIGGER IF EXISTS entity_"+kind+"_update");
   exec("CREATE TRIGGER entity_"+std::string(kind)+"_insert AFTER INSERT ON decodes BEGIN "+add+" END;CREATE TRIGGER entity_"+kind+"_delete AFTER DELETE ON decodes BEGIN "+remove+" END;");
   // Update identity fields as a delete/add pair. Payload/distance-only updates keep references valid.
   exec("CREATE TRIGGER entity_"+std::string(kind)+"_update AFTER UPDATE OF call,country,message,utc ON decodes WHEN OLD.call!=NEW.call OR coalesce(OLD.country,'')!=coalesce(NEW.country,'') OR OLD.message!=NEW.message OR OLD.utc!=NEW.utc BEGIN "+remove+add+" END;");
  }
  setSetting("entity_schema",2);exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
}
Store::RetentionState Store::retentionState(){
 RetentionState out;auto ui=getSetting("ui",Json::object());out.enabled=ui.value("history_auto_trim",true);out.limit=std::clamp(ui.value("history_max_records",int64_t(defaultHistoryLimit)),int64_t(10000),int64_t(1000000));out.target=historyCleanupTarget(out.limit,ui.value("history_trim_to",int64_t(0)));out.deleted=getSetting("retention_deleted",int64_t(0)).get<int64_t>();out.running=out.enabled&&!getSetting("retention_job",Json::object()).empty();
 auto s=prepare(db_,"SELECT (SELECT utc FROM decodes ORDER BY utc LIMIT 1),(SELECT utc FROM decodes ORDER BY utc DESC LIMIT 1)");if(step(db_,s.get())==SQLITE_ROW){out.firstUtc=sqlite3_column_int64(s.get(),0);out.lastUtc=sqlite3_column_int64(s.get(),1);}return out;
}
int64_t Store::retain(int64_t limit,int batch,int64_t syncedThrough,int64_t trimTo){
 if(limit<1||batch<1||batch>256)throw std::runtime_error("Invalid retention limit");
 const auto target=historyCleanupTarget(limit,trimTo);auto job=getSetting("retention_job",Json::object());auto count=savedMessageCount();
 if(!job.empty()&&job.value("limit",int64_t(0))!=limit){job=Json::object();setSetting("retention_job",job);}
 if(job.empty()){if(count<=limit)return 0;job={{"remaining",count-target},{"through",lastReceptionId()},{"limit",limit},{"target",target}};}
 if(job.value("target",historyCleanupTarget(limit))!=target){job["target"]=target;job["remaining"]=std::max<int64_t>(0,count-target);}
 auto remaining=std::min(job.at("remaining").get<int64_t>(),std::max<int64_t>(0,count-target));if(!remaining){setSetting("retention_job",Json::object());return 0;}auto through=std::min(job.at("through").get<int64_t>(),syncedThrough);
 std::vector<std::pair<int64_t,Decode>> expired;exec("BEGIN IMMEDIATE");int64_t removed=0;try{
  auto read=prepare(db_,"SELECT reception_id,call,dial,mode,country,grid,utc,payload FROM decodes INDEXED BY by_time WHERE reception_id<=? ORDER BY utc,reception_id LIMIT ?");sqlite3_bind_int64(read.get(),1,through);sqlite3_bind_int64(read.get(),2,std::min<int64_t>(batch,remaining));
  std::vector<int64_t> ids;std::map<std::tuple<std::string,int64_t,std::string>,int64_t> stations;std::set<std::pair<std::string,std::string>> heard;int64_t floor=0;
  while(step(db_,read.get())==SQLITE_ROW){auto id=sqlite3_column_int64(read.get(),0);ids.push_back(id);if(sessionCache_.after>=0||historyCache_.after>=0)expired.emplace_back(id,Decode::fromJson(Json::parse(text(read.get(),7))));++stations[{text(read.get(),1),sqlite3_column_int64(read.get(),2),text(read.get(),3)}];heard.emplace("call",text(read.get(),1));heard.emplace("country",text(read.get(),4));heard.emplace("grid",text(read.get(),5));floor=std::max<int64_t>(floor,sqlite3_column_int64(read.get(),6));}read.reset();
  auto erase=prepare(db_,"DELETE FROM decodes WHERE reception_id=?");for(auto id:ids){sqlite3_reset(erase.get());sqlite3_bind_int64(erase.get(),1,id);step(db_,erase.get());removed+=sqlite3_changes(db_);}
  for(const auto& [station,total]:stations){const auto& [call,dial,mode]=station;if(call.empty())continue;auto update=prepare(db_,"UPDATE stations SET utc=coalesce((SELECT utc FROM decodes WHERE call=? AND dial=? AND mode=? ORDER BY utc DESC,reception_id DESC LIMIT 1),utc),payload=coalesce((SELECT payload FROM decodes WHERE call=? AND dial=? AND mode=? ORDER BY utc DESC,reception_id DESC LIMIT 1),payload),count=count-? WHERE call=? AND dial=? AND mode=?");for(int i=0;i<2;i++){bind(update.get(),i*3+1,call);sqlite3_bind_int64(update.get(),i*3+2,dial);bind(update.get(),i*3+3,mode);}sqlite3_bind_int64(update.get(),7,total);bind(update.get(),8,call);sqlite3_bind_int64(update.get(),9,dial);bind(update.get(),10,mode);step(db_,update.get());}
  exec("DELETE FROM stations WHERE count=0");
  for(const auto& [kind,value]:heard){if(value.empty())continue;auto field=kind=="call"?"call":kind=="country"?"country":"grid";auto index=kind=="call"?"by_call_time":kind=="country"?"by_country":"by_grid";auto update=prepare(db_,"UPDATE heard SET utc=(SELECT min(utc) FROM decodes INDEXED BY "+std::string(index)+" WHERE "+field+"=?"+(kind=="grid"?" AND json_extract(payload,'$.grid_source')='received'":"")+") WHERE kind=? AND value=?");bind(update.get(),1,value);bind(update.get(),2,kind);bind(update.get(),3,value);step(db_,update.get());}exec("DELETE FROM heard WHERE utc IS NULL");
  auto counter=prepare(db_,"UPDATE counters SET value=value-? WHERE key='messages'");sqlite3_bind_int64(counter.get(),1,removed);step(db_,counter.get());
  remaining-=removed;job["remaining"]=remaining;if(remaining<=0||ids.empty())job=Json::object();setSetting("retention_job",job);setSetting("retention_deleted",getSetting("retention_deleted",int64_t(0)).get<int64_t>()+removed);
  if(removed){setSetting("retention_floor",std::max(getSetting("retention_floor",int64_t(0)).get<int64_t>(),floor));auto far=prepare(db_,"SELECT km,country FROM decodes WHERE km IS NOT NULL AND json_extract(payload,'$.grid_source')='received' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0)) ORDER BY km DESC,utc DESC,reception_id DESC LIMIT 1");bool found=step(db_,far.get())==SQLITE_ROW;setSetting("farthest_km",found?sqlite3_column_double(far.get(),0):0.);setSetting("farthest_country",found?text(far.get(),1):"");}
  exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}expireSession(expired);return removed;
}
bool Store::reclaimPages(int pages){
 if(pages<1||pages>128)throw std::runtime_error("Invalid compaction batch");auto mode=prepare(db_,"PRAGMA auto_vacuum");if(step(db_,mode.get())!=SQLITE_ROW||sqlite3_column_int(mode.get(),0)!=2)return false;mode.reset();exec("PRAGMA incremental_vacuum("+std::to_string(pages)+")");auto free=prepare(db_,"PRAGMA freelist_count");return step(db_,free.get())==SQLITE_ROW&&sqlite3_column_int64(free.get(),0)>0;
}
}
