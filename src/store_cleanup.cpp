// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include "helper_client.h"
#include <memory>
#include <stdexcept>
namespace dx {
namespace {
using Statement=std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)>;
Statement prepare(sqlite3* db,const std::string& sql){sqlite3_stmt* statement=nullptr;if(sqlite3_prepare_v2(db,sql.c_str(),-1,&statement,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));return {statement,sqlite3_finalize};}
std::string value(sqlite3_stmt* s,int col){auto text=sqlite3_column_text(s,col);return text?reinterpret_cast<const char*>(text):"";}
std::string condition(const SavedCleanup& s){
 switch(s.kind){
  case CleanupKind::Before:return "utc<?";
  case CleanupKind::After:return "utc>?";
  case CleanupKind::Between:return "utc BETWEEN ? AND ?";
  case CleanupKind::Country:return "country=?";
  case CleanupKind::ExceptCountry:return "coalesce(country,'')!=?";
  case CleanupKind::MissingDistance:return s.includeCached?"(km IS NULL OR coalesce(json_extract(payload,'$.grid_source'),'')!='received')":"km IS NULL";
  case CleanupKind::MissingGrid:return "coalesce(grid,'')=''";
 }throw std::runtime_error("Unknown saved-message cleanup");
}
void bind(sqlite3_stmt* statement,const SavedCleanup& s,int64_t throughId){
 sqlite3_bind_int64(statement,1,throughId);
 if(int(s.kind)<=2){sqlite3_bind_int64(statement,2,s.from);if(s.kind==CleanupKind::Between)sqlite3_bind_int64(statement,3,s.to);}
 if(s.kind==CleanupKind::Country||s.kind==CleanupKind::ExceptCountry){auto country=countryName(s.country);sqlite3_bind_text(statement,2,country.c_str(),int(country.size()),SQLITE_TRANSIENT);}
}
}
void Store::ensureReceptionIds(){
 bool explicitId=false;{auto columns=prepare(db_,"PRAGMA table_info(decodes)");while(sqlite3_step(columns.get())==SQLITE_ROW)if(value(columns.get(),1)=="reception_id"&&sqlite3_column_int(columns.get(),5)==1)explicitId=true;}
 if(explicitId)return;
 // An explicit integer primary key keeps CSV/import watermarks stable through VACUUM.
 std::vector<std::string> indexes;std::string schema;
 {auto read=prepare(db_,"SELECT sql FROM sqlite_master WHERE tbl_name='decodes' AND sql IS NOT NULL AND type='index'");while(sqlite3_step(read.get())==SQLITE_ROW)indexes.push_back(value(read.get(),0));auto table=prepare(db_,"SELECT sql FROM sqlite_master WHERE name='decodes' AND type='table'");if(sqlite3_step(table.get())==SQLITE_ROW)schema=value(table.get(),0);}
 auto start=schema.find('(');auto primary=schema.find("key TEXT PRIMARY KEY",start);if(start==std::string::npos||primary==std::string::npos)throw std::runtime_error("Reception schema cannot preserve IDs during compaction");
 schema.replace(primary,20,"reception_id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE");schema="CREATE TABLE decodes_stable_ids "+schema.substr(start);
 exec("BEGIN IMMEDIATE");try{exec(schema);exec("INSERT INTO decodes_stable_ids SELECT rowid,* FROM decodes;DROP TABLE decodes;ALTER TABLE decodes_stable_ids RENAME TO decodes");for(const auto& index:indexes)exec(index);exec("COMMIT");}catch(...){exec("ROLLBACK");throw;}
}
void Store::compact(){exec("PRAGMA wal_checkpoint(TRUNCATE)");exec("PRAGMA auto_vacuum=INCREMENTAL;VACUUM");exec("PRAGMA optimize");}
void Store::rebuildSummaries(bool stations){
 invalidateSession();
 if(stations)exec("DELETE FROM stations;INSERT INTO stations SELECT call,dial,mode,utc,total,payload FROM (SELECT call,dial,mode,utc,payload,count(*) OVER (PARTITION BY call,dial,mode) AS total,row_number() OVER (PARTITION BY call,dial,mode ORDER BY utc DESC,rowid DESC) AS latest FROM decodes WHERE call!='') WHERE latest=1");
 exec("DELETE FROM heard;INSERT INTO heard SELECT 'call',call,min(utc) FROM decodes WHERE call!='' AND call NOT LIKE '<%' GROUP BY call;INSERT INTO heard SELECT 'country',country,min(utc) FROM decodes WHERE country!='' GROUP BY country;INSERT INTO heard SELECT 'grid',grid,min(utc) FROM decodes WHERE grid!='' AND json_extract(payload,'$.grid_source')='received' GROUP BY grid;UPDATE counters SET value=(SELECT count(*) FROM decodes) WHERE key='messages'");
 auto farthest=prepare(db_,"SELECT km,country FROM decodes WHERE km IS NOT NULL AND json_extract(payload,'$.grid_source')='received' AND NOT dx_decode_suspect(mode,json_extract(payload,'$.raw'),coalesce(json_extract(payload,'$.decoder_suspect'),0)) ORDER BY km DESC,utc DESC,rowid DESC LIMIT 1");bool found=sqlite3_step(farthest.get())==SQLITE_ROW;setSetting("farthest_km",found?sqlite3_column_double(farthest.get(),0):0.);setSetting("farthest_country",found?value(farthest.get(),1):"");
}
std::vector<std::string> Store::savedCountries(){auto s=prepare(db_,"SELECT value FROM heard WHERE kind='country' ORDER BY value");std::vector<std::string> out;while(sqlite3_step(s.get())==SQLITE_ROW)out.push_back(value(s.get(),0));return out;}
int64_t Store::savedMessageCount(){auto s=prepare(db_,"SELECT value FROM counters WHERE key='messages'");return sqlite3_step(s.get())==SQLITE_ROW?sqlite3_column_int64(s.get(),0):0;}
int64_t Store::countCleanup(const SavedCleanup& cleanup,int64_t throughId){if(!cleanup.valid()||throughId<0)throw std::runtime_error("Invalid saved-message cleanup");auto s=prepare(db_,"SELECT count(*) FROM decodes WHERE rowid<=? AND "+condition(cleanup));bind(s.get(),cleanup,throughId);if(sqlite3_step(s.get())!=SQLITE_ROW)throw std::runtime_error(sqlite3_errmsg(db_));return sqlite3_column_int64(s.get(),0);}
int64_t Store::deleteSaved(const SavedCleanup& cleanup,int64_t throughId){
 if(!cleanup.valid()||throughId<0)throw std::runtime_error("Invalid saved-message cleanup");int64_t removed=0;
 exec("BEGIN IMMEDIATE");try{
  {auto watermark=lastReceptionId();auto counter=prepare(db_,"INSERT OR REPLACE INTO counters VALUES('reception_id',?)");sqlite3_bind_int64(counter.get(),1,watermark);if(sqlite3_step(counter.get())!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));
   auto remove=prepare(db_,"DELETE FROM decodes WHERE rowid<=? AND "+condition(cleanup));bind(remove.get(),cleanup,throughId);if(sqlite3_step(remove.get())!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));removed=sqlite3_changes(db_);
   if(removed)rebuildSummaries();
   // Persist CSV cleanup before commit so interruptions cannot leave silent leftovers.
   auto jobs=getSetting("cleanup_jobs",Json::object());auto key=std::to_string(monotonicMs())+"-"+std::to_string(throughId);while(jobs.contains(key))key+='x';auto paths=getSetting("csv_paths",Json::array());auto ui=getSetting("ui",Json::object());if(ui.contains("csv_path"))paths.push_back(ui["csv_path"]);jobs[key]={{"criteria",cleanup.json()},{"through_id",throughId},{"paths",paths},{"compact_pending",removed>0}};setSetting("cleanup_jobs",jobs);
  }exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
 return removed;
}
}
