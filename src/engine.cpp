// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "modes.h"
#include <set>
#include <sys/stat.h>
#include <fstream>
#include <regex>
#include <chrono>
#include <cmath>
namespace dx {
namespace {
int64_t now(){return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());}
void hideIgnoredViews(Snapshot& view,const IgnoreTarget& target){
 auto rows=[&](auto& list){list.erase(std::remove_if(list.begin(),list.end(),[&](const Row& row){return target.matches(row.decode);}),list.end());};rows(view.table);rows(view.map);
 auto messages=[&](auto& list){list.erase(std::remove_if(list.begin(),list.end(),[&](const Decode& d){return target.matches(d);}),list.end());};messages(view.details);messages(view.hoverMessages);
 if(view.hoverStation&&target.matches(*view.hoverStation))view.hoverStation.reset();
 view.paths.erase(std::remove_if(view.paths.begin(),view.paths.end(),[&](const ExchangePath& path){return !target.message&&(path.transmitter==target.value||path.called==target.value);}),view.paths.end());
 view.cornerReceptions.erase(std::remove_if(view.cornerReceptions.begin(),view.cornerReceptions.end(),[&](const CornerReception& row){return target.matches(row.decode);}),view.cornerReceptions.end());
 view.mapHeardCountries.clear();for(const auto& row:view.map)if(!row.decode.country.empty())view.mapHeardCountries.push_back(row.decode.country);std::sort(view.mapHeardCountries.begin(),view.mapHeardCountries.end());view.mapHeardCountries.erase(std::unique(view.mapHeardCountries.begin(),view.mapHeardCountries.end()),view.mapHeardCountries.end());
 view.moderating=true;
}
}
std::optional<Job> SlotCapture::feed(const float* samples,size_t count,int64_t endMs,int64_t dial,const std::string& mode,int periodMs,int submode){
 auto duration=capturePeriodMs(mode,periodMs);if(!duration){reset();return {};}auto expected=size_t(*duration)*12;if(expected!=periodSamples_||mode_!=mode||dial_!=dial||submode_!=submode)reset();periodSamples_=expected;mode_=mode;dial_=dial;submode_=submode;
 samples_+=count;int64_t endSample=endMs*12,start=endSample-int64_t(count);
 if(nextSample_>=0){if(std::abs(start-nextSample_)>2400){++discontinuities_;reset();}else start=nextSample_;}
 nextSample_=start+int64_t(count);std::optional<Job> result;
 for(size_t i=0;i<count;i++){int64_t sample=start+int64_t(i),slot=sample/int64_t(periodSamples_),pos=sample%int64_t(periodSamples_);
  if(slot!=slot_){pcm_.clear();slot_=slot;}
  if(pos==0)pcm_.reserve(periodSamples_);
  if(pos==int64_t(pcm_.size())){float v=std::isfinite(samples[i])?samples[i]:0;pcm_.push_back(int16_t(std::clamp(v*16383.52f,-32767.f,32767.f)));}
  if(pcm_.size()==periodSamples_){Job j;j.utcMs=slot*int64_t(periodSamples_)/12;j.dial=dial;j.mode=mode;j.periodMs=int(*duration);j.submode=submode;j.pcm=std::move(pcm_);j.deadlineMs=monotonicMs()+*duration;result=std::move(j);pcm_.clear();}
 }return result;
}
Engine::Engine(std::filesystem::path root,std::filesystem::path helper,Point home,std::function<std::pair<std::string,std::string>(const std::string&)> country):root_(std::move(root)),worldFile_(helper.parent_path()/"world.geojson"),helper_(helper),home_(home),country_(std::move(country)),modeHelper_(platform::executable(helper.parent_path()/"dx_modes_decoder"),true),js8Helper_(platform::executable(helper.parent_path()/"dx_js8_decoder"),true,20000){
 std::filesystem::create_directories(root_);startedAt_=now();table_.uniqueCalls=true;
 decoder_=std::thread(&Engine::decodeLoop,this);modeDecoder_=std::thread(&Engine::modeLoop,this);js8Decoder_=std::thread(&Engine::js8Loop,this);writer_=std::thread(&Engine::storeLoop,this);
}
Engine::~Engine(){stopping_=true;modeCancel_=true;js8Cancel_=true;cv_.notify_all();decoder_.join();modeDecoder_.join();js8Decoder_.join();writer_.join();}
void Engine::enqueue(Job job){static std::atomic<uint64_t> id{1};job.id=id++;job.decodeDepth=decodeDepth_.load();std::lock_guard<std::mutex> lock(mutex_);
 if(job.mode=="JS8"){if(js8Jobs_.size()>=js8QueueLimit||queuedAudioBytes(js8Jobs_,&job)>js8QueueBytes){++status_.skipped;return;}job.context=modeContext_;js8Jobs_.push_back(std::move(job));std::stable_sort(js8Jobs_.begin(),js8Jobs_.end(),[](const auto& a,const auto& b){return a.deadlineMs<b.deadlineMs;});cv_.notify_all();return;}
 if(job.mode!="FT8"&&job.mode!="FT4"){if(modeJobs_.size()>=additionalQueueLimit||queuedAudioBytes(modeJobs_,&job)>additionalQueueBytes){++status_.skipped;return;}job.context=modeContext_;modeJobs_.push_back(std::move(job));std::stable_sort(modeJobs_.begin(),modeJobs_.end(),[](const auto& a,const auto& b){return a.deadlineMs<b.deadlineMs;});cv_.notify_all();return;}
 if(jobs_.size()>=32||blocked_){++status_.skipped;return;}jobs_.push_back(std::move(job));std::stable_sort(jobs_.begin(),jobs_.end(),[](auto& a,auto& b){return a.deadlineMs<b.deadlineMs;});cv_.notify_all();}
void Engine::query(Query t,Query m,uint64_t g){std::lock_guard<std::mutex> lock(mutex_);if(t.countries)t.messages=false;t.uniqueCalls=!t.messages;table_=std::move(t);map_=std::move(m);generation_=g;queryDirty_=true;cv_.notify_all();}
void Engine::refresh(Query t,Query m){std::lock_guard<std::mutex> lock(mutex_);if(t.countries)t.messages=false;t.uniqueCalls=!t.messages;table_=std::move(t);map_=std::move(m);queryDirty_=true;cv_.notify_all();}
void Engine::select(const Decode& d,int offset){std::lock_guard<std::mutex> lock(mutex_);selection_=d;selectionCountry_.clear();historyOffset_=offset;selectionDirty_=true;cv_.notify_all();}
void Engine::selectCountry(std::string country,int offset){std::lock_guard<std::mutex> lock(mutex_);selection_.reset();selectionCountry_=std::move(country);historyOffset_=offset;selectionDirty_=true;cv_.notify_all();}
void Engine::hover(const Decode& d){std::lock_guard<std::mutex> lock(mutex_);if(hovered_&&hovered_->identity.transmitter==d.identity.transmitter&&hovered_->dial==d.dial&&hovered_->mode==d.mode)return;hovered_=d;selectionDirty_=true;cv_.notify_all();}
void Engine::settings(Json values){std::lock_guard<std::mutex> lock(mutex_);pendingSettings_.merge_patch(values);cv_.notify_all();}
uint64_t Engine::restoreDefaults(){std::lock_guard<std::mutex> lock(mutex_);auto id=uint64_t(monotonicMs());pendingDefaults_=id;pendingSettings_=Json::object();cv_.notify_all();return id;}
void Engine::exportData(Query q,std::filesystem::path p,bool csv){std::lock_guard<std::mutex> lock(mutex_);export_=std::make_tuple(q,std::move(p),csv);cv_.notify_all();}
void Engine::active(bool enabled){std::lock_guard<std::mutex> lock(mutex_);active_=enabled;}
void Engine::alertContext(std::string bands,bool ft8,bool ft4,std::string modes){std::lock_guard<std::mutex> lock(mutex_);alertBands_=std::move(bands);alertModes_=std::move(modes);alertFT8_=ft8;alertFT4_=ft4;}
void Engine::home(Point point){std::lock_guard<std::mutex> lock(mutex_);pendingHome_=point;queryDirty_=true;cv_.notify_all();}
void Engine::resetStats(){std::lock_guard<std::mutex> lock(mutex_);pendingStatsReset_=true;queryDirty_=true;cv_.notify_all();}
void Engine::hideIgnored(const IgnoreTarget& target){auto next=std::make_shared<Snapshot>(*snapshot_);hideIgnoredViews(*next,target);snapshot_=std::move(next);}
uint64_t Engine::moderate(IgnoreTarget target,bool deleteRecords,bool ignoreFuture){
 if(target.value.empty()||target.value.find('\0')!=std::string::npos||(!deleteRecords&&!ignoreFuture))return 0;
 target.value=upper(std::move(target.value));std::lock_guard<std::mutex> lock(mutex_);auto id=++nextModeration_;moderation_.push_back({target,deleteRecords,ignoreFuture,id});if(deleteRecords)hideIgnored(target);queryDirty_=true;cv_.notify_all();return id;
}
void Engine::ignoreTransmitter(std::string call,bool deleteRecords){moderate({std::move(call),false},deleteRecords,true);}
void Engine::ignoreMessage(std::string message,bool deleteRecords){moderate({std::move(message),true},deleteRecords,true);}
void Engine::unignoreAll(){std::lock_guard<std::mutex> lock(mutex_);moderation_.push_back({{},false,false,++nextModeration_});queryDirty_=true;cv_.notify_all();}
void Engine::gridTrackerContext(GridTrackerContext context){std::lock_guard<std::mutex> lock(mutex_);gridTrackerContext_=std::move(context);}
void Engine::retry(){std::lock_guard<std::mutex> lock(mutex_);retry_=true;blocked_=false;status_.error.clear();cv_.notify_all();}
std::shared_ptr<const Snapshot> Engine::snapshot()const{std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
void Engine::invalidateAdditional(){std::lock_guard<std::mutex> lock(mutex_);++modeContext_;modeJobs_.clear();js8Jobs_.clear();status_.js8Error.clear();modeCancel_=true;js8Cancel_=true;cv_.notify_all();}
void Engine::modeLoop(){
 while(!stopping_){Job job;{
  std::unique_lock<std::mutex> lock(mutex_);cv_.wait_for(lock,std::chrono::milliseconds(200),[&]{return stopping_||!modeJobs_.empty()||modeCancel_;});if(stopping_)break;
  if(modeCancel_){modeHelper_.stop();modeCancel_=false;}if(modeJobs_.empty())continue;job=std::move(modeJobs_.front());modeJobs_.pop_front();if(monotonicMs()>job.deadlineMs){++status_.skipped;continue;}status_.modeDecoding=true;
 }
 try{auto response=modeHelper_.decode(job,modeCancel_);std::vector<Decode> decoded;
  for(auto& row:response.at("results")){row["timestamp"]=job.utcMs/1000;row["slot_ms"]=job.utcMs;row["frequency"]=job.dial;row["mode"]=job.mode;row["source"]=job.source;decoded.push_back(Decode::fromJson(row));}
  std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stopping_||job.context!=modeContext_||writes_.size()+decoded.size()<=2000;});if(!stopping_&&job.context==modeContext_)for(auto& row:decoded)writes_.push_back(std::move(row));status_.modeError.clear();++status_.decodedJobs;
 }catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex_);if(!stopping_&&job.context==modeContext_){status_.modeError=job.mode+": "+e.what();++status_.failures;++status_.skipped;}}
 {std::lock_guard<std::mutex> lock(mutex_);status_.modeDecoding=false;cv_.notify_all();}
 }modeHelper_.stop();modeDone_=true;cv_.notify_all();
}
void Engine::decodeLoop(){int consecutive=0;
 while(!stopping_){Job job;{
  std::unique_lock<std::mutex> lock(mutex_);cv_.wait_for(lock,std::chrono::milliseconds(200),[&]{return stopping_||(!blocked_&&!jobs_.empty())||retry_;});if(stopping_)break;if(retry_){consecutive=0;retry_=false;}if(blocked_||jobs_.empty())continue;
  job=std::move(jobs_.front());jobs_.pop_front();if(monotonicMs()>job.deadlineMs){++status_.skipped;continue;}status_.decoding=true;
 }
 auto start=monotonicMs();try{
  auto response=helper_.decode(job,stopping_);std::vector<Decode> decoded;
  for(auto& row:response.at("results")){row["timestamp"]=job.utcMs/1000;row["slot_ms"]=job.utcMs;row["frequency"]=job.dial;row["mode"]=job.mode;row["source"]=job.source;decoded.push_back(Decode::fromJson(row));}
  std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stopping_||writes_.size()+decoded.size()<=2000;});for(auto& d:decoded)writes_.push_back(std::move(d));status_.decoding=false;status_.helperPid=helper_.pid();status_.lastDecodeMs=double(monotonicMs()-start);++status_.decodedJobs;status_.error.clear();consecutive=0;cv_.notify_all();
 }catch(const std::exception& e){std::unique_lock<std::mutex> lock(mutex_);status_.decoding=false;status_.helperPid=-1;++status_.failures;status_.error=e.what();++consecutive;
  if(!stopping_&&job.retry==0&&monotonicMs()+500<job.deadlineMs){job.retry=1;jobs_.push_front(std::move(job));}else ++status_.skipped;
  if(consecutive>=3){blocked_=true;status_.error+="; Retry decoder";}else cv_.wait_for(lock,std::chrono::milliseconds(500*(1<<consecutive)),[&]{return stopping_.load();});
 }}helper_.stop();decoderDone_=true;cv_.notify_all();
}
void Engine::importText(Store& store){
 if(allTxt_.empty())return;struct stat st{};if(stat(allTxt_.c_str(),&st)!=0)return;
 if(uint64_t(st.st_ino)!=logInode_||st.st_size<logPosition_){logPosition_=0;logAnchor_.clear();logInode_=uint64_t(st.st_ino);}
 std::ifstream input(allTxt_,std::ios::binary);if(!input)return;
 if(logPosition_>0&&!logAnchor_.empty()){input.seekg(logPosition_-int64_t(logAnchor_.size()));std::string anchor(logAnchor_.size(),'\0');input.read(anchor.data(),anchor.size());if(anchor!=logAnchor_)logPosition_=0;}
 input.clear();input.seekg(logPosition_);static const std::regex line("^(\\d{6}_\\d{6})\\s+(\\d+\\.\\d+)\\s+Rx\\s+(FT[48])\\s+(-?\\d+)\\s+(-?\\d+(?:\\.\\d+)?)\\s+(\\d+)\\s+(.+)$");
 for(int n=0;n<500&&!stopping_;n++){std::string text;int64_t before=logPosition_;if(!std::getline(input,text)||input.eof())break;auto next=input.tellg();if(text.size()>65536){logPosition_=int64_t(next);continue;}std::smatch match;
  if(std::regex_match(text,match,line)){
   tm stamp{};std::istringstream date(match[1].str());date>>std::get_time(&stamp,"%y%m%d_%H%M%S");if(!date.fail()){
    Decode d;d.utc=platform::utcEpoch(&stamp);d.slotMs=d.utc*1000;d.dial=int64_t(std::llround(std::stod(match[2].str())*1e6));d.mode=match[3];d.snr=std::stoi(match[4]);d.audio=std::stoi(match[6]);d.message=match[7];d.source="legacy";d.raw=text;
    try{store.insert(d,home_,country_);}catch(...){logPosition_=before;throw;}
   }
  }logPosition_=int64_t(next);
 }
 input.clear();auto length=std::min<int64_t>(128,logPosition_);input.seekg(logPosition_-length);logAnchor_.assign(size_t(length),'\0');input.read(logAnchor_.data(),length);
 store.setSetting("log_cursor",{{"position",logPosition_},{"inode",logInode_},{"anchor",logAnchor_}});
}
void Engine::publish(Store& store){Query table,map;uint64_t generation;std::optional<Decode> selection,hovered;std::string selectedCountry;int offset;
 {std::lock_guard<std::mutex> lock(mutex_);table=table_;map=map_;generation=generation_;selection=selection_;selectedCountry=selectionCountry_;hovered=hovered_;offset=historyOffset_;queryDirty_=false;selectionDirty_=false;}
 map.filter.includeQuestionable=false;if(table.countries)table.filter.includeQuestionable=false;table.referenceUtc=map.referenceUtc=now();auto ui=store.getSetting("ui",Json::object());auto begin=monotonicMs();auto next=std::make_shared<Snapshot>();next->gridTracker=gridTracker_.state();next->historyAll=ui.value("history_all",false);next->statsHistory=next->historyAll||ui.value("stats_history",false);
 store.countryDistances(ui.value("approximate_country_distances",true),home_);
 if(next->historyAll){
  auto scope=[](Query& q,int64_t since){q.since=std::max(q.since,since);q.firstSince=since;q.afterRowId=q.firstAfterId=0;};
  scope(table,ui.value("history_table_cleared",int64_t(0)));scope(map,ui.value("history_map_cleared",int64_t(0)));
 }else{
  table.since=std::max(table.since,startedAt_);map.since=std::max(map.since,startedAt_);
  table.firstSince=std::max(table.firstSince,startedAt_);map.firstSince=std::max(map.firstSince,startedAt_);table.firstAfterId=map.firstAfterId=startAfterId_;
  if(table.since>=startedAt_)table.afterRowId=std::max(table.afterRowId,startAfterId_);if(map.since>=startedAt_)map.afterRowId=std::max(map.afterRowId,startAfterId_);
 }
 auto statsFilter=table.filter;statsFilter.includeQuestionable=false;auto session=store.sessionStats(now(),next->historyAll?0:statsAfterId_,next->historyAll?0:statsSince_,statsFilter);next->stats=next->statsHistory?store.stats(now(),statsFilter):session;next->stats.newestCountries=std::move(session.newestCountries);next->stats.activeSeconds=sessionSeconds_;next->tableMessages=table.messages;next->tableCountries=table.countries;if(table.countries){auto latest=table;latest.sort=0;latest.offset=0;next->countryTable=countrySummaries(store.rows(latest),table.sort,table.descending);next->tableCount=int64_t(next->countryTable.size());}else{next->table=store.rows(table,table.offset,500);if(next->table.size()<500&&(!next->table.empty()||table.offset==0))next->tableCount=table.offset+int64_t(next->table.size());else store.rows(table,0,0,&next->tableCount);}next->map=store.rows(map);store.mapLocations(next->map);next->paths=store.exchangePaths(map);if(next->map.size()<10000)next->mapCount=int64_t(next->map.size());else store.rows(map,0,0,&next->mapCount);
 for(const auto& row:next->map)if(!row.decode.country.empty())next->mapHeardCountries.push_back(row.decode.country);std::sort(next->mapHeardCountries.begin(),next->mapHeardCountries.end());next->mapHeardCountries.erase(std::unique(next->mapHeardCountries.begin(),next->mapHeardCountries.end()),next->mapHeardCountries.end());
 if(table.filter.json()!=Filter{}.json()){auto q=table;q.filter={};q.filter.includeQuestionable=table.filter.includeQuestionable;int64_t all=0;if(q.countries){q.sort=0;all=int64_t(countrySummaries(store.rows(q)).size());}else store.rows(q,0,0,&all);next->tableExcluded=all-next->tableCount;}
 if(map.filter.json()!=Filter{}.json()){auto q=map;q.filter={};int64_t all=0;store.rows(q,0,0,&all);next->mapExcluded=all-next->mapCount;}
 next->retention=store.retentionState();next->retentionError=status_.retentionError;next->historyBytes=status_.historyBytes;next->historyChecks=status_.historyChecks;
 // Cheap file metadata on the writer thread, refreshed with the view. The
 // 30-minute retention check and its database+journal size remain independent.
 std::error_code sizeError;auto sqliteBytes=std::filesystem::file_size(root_/"history.sqlite3",sizeError);if(!sizeError)next->sqliteBytes=sqliteBytes;
 next->savedCountries=store.savedCountries();next->savedMessages=store.savedMessageCount();next->ignoredCalls=store.ignoredCalls();next->ignoredMessages=store.ignoredMessages();next->moderating=store.getSetting("compact_pending",false).get<bool>()||!store.getSetting("cleanup_jobs",Json::object()).empty()||!store.getSetting("log_purges",Json::object()).empty()||!store.getSetting("message_log_purges",Json::object()).empty();
 auto csvPaths=store.getSetting("csv_paths",Json::array());for(const auto& path:csvPaths)if(path.is_string())next->csvPaths.push_back(path.get<std::string>());
 next->csvPaths.push_back(ui.value("csv_path",(root_/"receptions.csv").string()));if(std::filesystem::exists(root_/"receptions.csv"))next->csvPaths.push_back((root_/"receptions.csv").string());std::sort(next->csvPaths.begin(),next->csvPaths.end());next->csvPaths.erase(std::unique(next->csvPaths.begin(),next->csvPaths.end()),next->csvPaths.end());
 auto directionQuery=activityQuery(map,now());const auto stationCount=std::clamp(ui.value("direction_stations",5),1,25);
 // SQL selects the farthest matching unique senders before fetching payloads.
 next->directionStations=farthestActivity(home_,store.rows(directionQuery,0,stationCount),stationCount);next->direction=activityBearing(next->directionStations);
 if(selection){next->details=store.history(selection->identity.transmitter,selection->dial,selection->mode,offset);store.projectDistance(*selection);next->detailSelection=selection;}
 if(!selectedCountry.empty()){auto countryMessages=table;countryMessages.messages=true;countryMessages.countries=countryMessages.uniqueCalls=false;countryMessages.sort=18;countryMessages.descending=true;next->detailsCountry=selectedCountry;if(countryMessages.filter.country.empty()||matches(selectedCountry,countryMessages.filter.country)){countryMessages.filter.country=selectedCountry;for(const auto& row:store.rows(countryMessages,offset,500))next->details.push_back(row.decode);}}
 if(hovered){store.projectDistance(*hovered);next->hoverStation=hovered;next->hoverMessages=store.uniqueMessages(*hovered,map);}
 for(auto* rows:{&next->stats.latest,&next->stats.latestCountries,&next->details,&next->hoverMessages})for(auto& d:*rows)store.projectDistance(d);
 next->cornerReceptions=cornerFeed_.rows();next->cornerReceptions.erase(std::remove_if(next->cornerReceptions.begin(),next->cornerReceptions.end(),[&](const auto& row){return !store.passesLocationFilter(row.decode,table.filter);}),next->cornerReceptions.end());for(auto& row:next->cornerReceptions)store.projectDistance(row.decode);next->settings=store.getSetting("ui",Json::object());next->generation=generation;next->queryMs=double(monotonicMs()-begin);
 std::lock_guard<std::mutex> lock(mutex_);if(generation==generation_){for(const auto& action:moderation_)if(action.deleteRecords)hideIgnoredViews(*next,action.target);next->completedModeration=status_.completedModeration;next->cleanupPreview=status_.cleanupPreview;next->cleanupPreviewId=status_.cleanupPreviewId;next->cleanupMatches=status_.cleanupMatches;next->cleanupThroughId=status_.cleanupThroughId;next->completedCleanup=status_.completedCleanup;next->cleanupDeleted=status_.cleanupDeleted;next->cleanupBusy=status_.cleanupBusy;next->cleanupError=status_.cleanupError;next->moderating=next->moderating||!moderation_.empty();next->arrivals=status_.arrivals;next->newestReceptionUtc=status_.newestReceptionUtc;next->csvPath=status_.csvPath;next->csvBytes=status_.csvBytes;next->csvError=status_.csvError;next->csvCatchingUp=status_.csvCatchingUp;next->notice=status_.notice;next->noticeRevision=status_.noticeRevision;next->alert=status_.alert;next->alertUtc=status_.alertUtc;next->alertSound=status_.alertSound;next->alertTone=status_.alertTone;next->queued=jobs_.size();next->pendingWrites=writes_.size();next->skipped=status_.skipped;next->failures=status_.failures;next->decodedJobs=status_.decodedJobs;next->lastDecodeMs=status_.lastDecodeMs;next->helperPid=status_.helperPid;next->decoding=status_.decoding;next->error=status_.error;next->modeError=status_.modeError;next->modeDecoding=status_.modeDecoding;next->modeQueued=modeJobs_.size();next->js8Queued=js8Jobs_.size();next->js8Decoding=status_.js8Decoding;next->js8Error=status_.js8Error;snapshot_=std::move(next);}
}
void Engine::storeLoop(){
 std::unique_ptr<Store> store;double seconds=0;auto tick=monotonicMs(),lastPublish=tick,lastImport=tick,lastSave=tick;bool baseline=true;std::string lastLocationPolicy;
 for(;;){
  {std::lock_guard<std::mutex> lock(mutex_);if(stopping_&&decoderDone_&&modeDone_&&js8Done_&&writes_.empty()&&pendingSettings_.empty()&&!pendingDefaults_&&moderation_.empty()&&cleanups_.empty()&&!cleanupPreview_)break;if(pendingHome_){home_=*pendingHome_;pendingHome_.reset();}}
  try{
  if(!store){store=std::make_unique<Store>(root_/"history.sqlite3");store->loadCountryAreas(worldFile_);store->reclassifyCountries(country_);startAfterId_=arrivalCursor_=statsAfterId_=store->lastReceptionId();statsSince_=startedAt_;seconds=store->getSetting("active_seconds",0.).get<double>();auto c=store->getSetting("log_cursor",Json::object());logPosition_=c.value("position",int64_t(0));logInode_=c.value("inode",uint64_t(0));logAnchor_=c.value("anchor","");
#if DX_HOST_BROWN
   auto config=root_.parent_path()/"sdrpp-brown"/"ft8_decoder_config.json";std::ifstream f(config);Json j=Json::parse(f,nullptr,false);if(!j.is_discarded())for(auto& entry:j.items())if(entry.value().contains("allTxtPath"))allTxt_=entry.value().value("allTxtPath","");
#endif
  }
  store->rebaseDistances(home_);
  {std::lock_guard<std::mutex> lock(mutex_);if(pendingStatsReset_){statsAfterId_=store->lastReceptionId();statsSince_=now();sessionSeconds_=0;status_.newestReceptionUtc=0;status_.arrivals.clear();cornerFeed_.clear();pendingStatsReset_=false;}}
  std::deque<Decode> batch;Json settings;std::optional<uint64_t> defaults;std::optional<std::tuple<Query,std::filesystem::path,bool>> exportJob;bool active,dirty;
  {std::lock_guard<std::mutex> lock(mutex_);size_t count=std::min<size_t>(100,writes_.size());for(size_t i=0;i<count;i++)batch.push_back(writes_[i]);settings=std::move(pendingSettings_);pendingSettings_=Json::object();defaults=pendingDefaults_;pendingDefaults_.reset();exportJob=std::move(export_);export_.reset();active=active_;dirty=queryDirty_||selectionDirty_;}
  if(defaults){store->setSetting("ui",{{"defaults_revision",*defaults}});settings=Json::object();retentionCheckAt_=lastBaseline_=0;dirty=true;std::lock_guard<std::mutex> lock(mutex_);++status_.noticeRevision;status_.notice="Plugin settings restored to defaults";}
  std::optional<Moderation> moderation;{std::lock_guard<std::mutex> lock(mutex_);if(!moderation_.empty())moderation=moderation_.front();}
  if(moderation){
   const auto& target=moderation->target;if(moderation->deleteRecords)cornerFeed_.remove(target);
   if(target.value.empty())store->unignoreAll();else store->moderate(target,moderation->deleteRecords,moderation->ignoreFuture);lastBaseline_=0;dirty=true;
   auto remaining=store->sessionStats(now(),statsAfterId_,statsSince_);
   std::lock_guard<std::mutex> lock(mutex_);moderation_.pop_front();status_.completedModeration=moderation->id;
   if(moderation->deleteRecords){status_.arrivals.clear();status_.newestReceptionUtc=remaining.latest.empty()?0:remaining.latest.front().utc;status_.alert.clear();if(selection_&&target.matches(*selection_))selection_.reset();if(hovered_&&target.matches(*hovered_))hovered_.reset();}
   ++status_.noticeRevision;status_.notice=target.value.empty()?"Future receptions allowed; deleted history stays deleted":moderation->deleteRecords?"Removing deleted receptions from saved logs":"Future receptions blocked; saved receptions kept";
  }
  auto ui=store->getSetting("ui",Json::object());decodeDepth_=std::clamp(ui.value("ft_decode_depth",defaultFtDecodeDepth),1,3);auto policyJson=ui.value("alerts",Json::object());AlertPolicy policy;policy.miles=ui.value("distance_miles",false);policy.filter=Filter::fromJson(policyJson.value("filter",Json::object()));policy.seconds=policyJson.value("seconds",int64_t(3600));policy.sound=policyJson.value("sound",false);policy.sounds=policyJson.value("sounds",Json::object());policy.distance=policyJson.value("distance",true);policy.country=policyJson.value("country",false);policy.grid=policyJson.value("grid",false);policy.watch=policyJson.value("watch",false);policy.watchCalls=ui.value("watch_calls","");
  {std::lock_guard<std::mutex> lock(mutex_);if(policy.filter.band.empty()&&!policyJson.value("all_bands",true))policy.filter.band=alertBands_.empty()?"__none__":alertBands_;if(policy.filter.mode.empty())policy.filter.mode=!alertModes_.empty()?alertModes_:alertFT8_?(alertFT4_?"FT8,FT4":"FT8"):(alertFT4_?"FT4":"__none__");}
  Filter locationFilter;{std::lock_guard<std::mutex> lock(mutex_);locationFilter.removeLikelyFalse=table_.filter.removeLikelyFalse;locationFilter.falseToleranceKm=table_.filter.falseToleranceKm;}
  auto locationPolicy=locationFilter.json().dump();if(locationPolicy!=lastLocationPolicy){lastLocationPolicy=locationPolicy;lastBaseline_=0;std::lock_guard<std::mutex> lock(mutex_);status_.alert.clear();}
  policy.filter.includeQuestionable=locationFilter.includeQuestionable;policy.filter.removeLikelyFalse=locationFilter.removeLikelyFalse;policy.filter.falseToleranceKm=locationFilter.falseToleranceKm;
  if(now()-lastBaseline_>=60){Query q;q.since=policy.seconds?now()-policy.seconds:0;q.filter=policy.filter;alerts_.baseline(store->rows(q,0,100000));alerts_.suppressBefore(now()-60);lastBaseline_=now();}
  if(!batch.empty()){const bool history=ui.value("history_all",false)||ui.value("stats_history",false);alerts_.distanceBaseline(history?store->stats(now(),locationFilter).farthest:store->sessionStats(now(),statsAfterId_,statsSince_,locationFilter).farthest);}
  std::vector<Decode> fresh;size_t committed=0;try{for(auto& d:batch){if(store->insert(d,home_,country_))fresh.push_back(d);++committed;}}catch(...){std::lock_guard<std::mutex> lock(mutex_);while(committed--)writes_.pop_front();cv_.notify_all();throw;}
  auto alertFresh=fresh;alertFresh.erase(std::remove_if(alertFresh.begin(),alertFresh.end(),[&](const auto& d){return !store->passesLocationFilter(d,locationFilter);}),alertFresh.end());
  policy.station=policyJson.value("station",false);if(active)if(auto alert=alerts_.evaluate(alertFresh,policy,now())){std::lock_guard<std::mutex> lock(mutex_);status_.alert=alert->text;status_.alertUtc=alert->utc;status_.alertSound=alert->sound;status_.alertTone=alert->tone;}
  if(committed){std::lock_guard<std::mutex> lock(mutex_);while(committed--)writes_.pop_front();cv_.notify_all();dirty=true;}
  if(!settings.empty()){auto ui=store->getSetting("ui",Json::object());auto before=std::make_tuple(ui.value("history_auto_trim",true),ui.value("history_max_records",int64_t(defaultHistoryLimit)),ui.value("history_trim_to",int64_t(0)));ui.merge_patch(settings);store->setSetting("ui",ui);if(before!=std::make_tuple(ui.value("history_auto_trim",true),ui.value("history_max_records",int64_t(defaultHistoryLimit)),ui.value("history_trim_to",int64_t(0))))retentionCheckAt_=0;dirty=true;}
 store->countryDistances(store->getSetting("ui",Json::object()).value("approximate_country_distances",true),home_);
 if(exportJob){store->exportRows(std::get<0>(*exportJob),std::get<1>(*exportJob),std::get<2>(*exportJob));std::lock_guard<std::mutex> lock(mutex_);++status_.noticeRevision;status_.notice="Export saved";}
  auto t=monotonicMs();if(active){auto elapsed=std::min<int64_t>(t-tick,1000)/1000.;seconds+=elapsed;sessionSeconds_+=elapsed;}tick=t;
  if(t-lastSave>1000){store->setSetting("active_seconds",seconds);lastSave=t;}
#if DX_HOST_BROWN
  if(t-lastImport>250){auto added=store->importBridge(root_.parent_path()/"FT8 Map"/"history.sqlite3",home_,country_);importText(*store);lastImport=t;dirty=dirty||added>0;baseline=false;}
#endif
  for(auto& entry:store->logRows(arrivalCursor_,1000)){arrivalCursor_=entry.first;auto& d=entry.second;if(d.source=="legacy"&&d.utc<startedAt_)continue;if(!d.diagnostics.suspect)cornerFeed_.add(entry.first,d,t);std::lock_guard<std::mutex> lock(mutex_);status_.arrivals.push_back({d.key(),t});status_.newestReceptionUtc=std::max(status_.newestReceptionUtc,d.utc);dirty=true;}
  {std::lock_guard<std::mutex> lock(mutex_);auto& arrivals=status_.arrivals;arrivals.erase(std::remove_if(arrivals.begin(),arrivals.end(),[&](const Arrival& a){return t-a.observedMs>=int64_t(arrivalHighlightSeconds*1000);}),arrivals.end());}
  dirty=manageSaved(*store)||dirty;
  auto logSettings=store->getSetting("ui",Json::object());auto cornerCount=cornerFeed_.rows().size();cornerFeed_.expire(t,logSettings.value("corner_seconds",10));dirty=dirty||cornerFeed_.rows().size()!=cornerCount;auto logPath=logSettings.value("csv_path",(root_/"receptions.csv").string());
  auto paths=store->getSetting("csv_paths",Json::array());paths.push_back(logPath);if(std::filesystem::exists(root_/"receptions.csv"))paths.push_back((root_/"receptions.csv").string());std::set<std::string> logs;for(const auto& path:paths)if(path.is_string())logs.insert(path.get<std::string>());std::string purgeError;
  auto clean=[&](const char* key,bool message){auto pending=store->getSetting(key,Json::object());for(const auto& entry:pending.items()){
   bool complete=true;for(const auto& path:logs)try{csv_.purge(*store,path,IgnoreTarget{entry.key(),message},entry.value().get<int64_t>());}catch(const std::exception& e){complete=false;purgeError="Log removal pending: "+path+": "+e.what();}
   if(complete){auto retained=store->getSetting(key,Json::object());retained.erase(entry.key());store->setSetting(key,retained);dirty=true;}
  }return !pending.empty();};
  bool purged=clean("log_purges",false);purged=clean("message_log_purges",true)||purged;
  if(purged&&store->getSetting("log_purges",Json::object()).empty()&&store->getSetting("message_log_purges",Json::object()).empty()){std::lock_guard<std::mutex> lock(mutex_);++status_.noticeRevision;status_.notice="Saved receptions deleted";}
  try{bool catchingUp=csv_.sync(*store,logPath,uint64_t(std::clamp(logSettings.value("csv_max_mb",100),1,4096))*1024*1024);std::lock_guard<std::mutex> lock(mutex_);status_.csvPath=logPath;status_.csvBytes=csv_.bytes();status_.csvCatchingUp=catchingUp;status_.csvError=purgeError;}catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex_);status_.csvPath=logPath;status_.csvError=purgeError.empty()?e.what():purgeError+"; "+e.what();}
  GridTrackerContext forwarding;{std::lock_guard<std::mutex> lock(mutex_);forwarding=gridTrackerContext_;}gridTracker_.tick(*store,logSettings.value("gridtracker",Json::object()),forwarding,active,now(),t);
  if(t>=retentionCheckAt_&&!stopping_){
   retentionCheckAt_=t+historyCheckIntervalMs;
   try{++status_.historyChecks;auto retention=store->retentionState();retentionWorking_=retention.enabled&&(retention.running||store->savedMessageCount()>retention.limit||retention.deleted>0);auto file=root_/"history.sqlite3",wal=root_/"history.sqlite3-wal";status_.historyBytes=std::filesystem::file_size(file)+(std::filesystem::exists(wal)?std::filesystem::file_size(wal):0);}
   catch(const std::exception& e){status_.retentionError=e.what();retentionCheckAt_=t+30000;}
  }
  if(retentionWorking_&&t>=retentionAt_&&!stopping_){
   bool ready;int64_t protectedThrough;{std::lock_guard<std::mutex> lock(mutex_);ready=!status_.cleanupBusy&&status_.csvError.empty();protectedThrough=csv_.cursor();}
   retentionAt_=t+250;
   // Reception writes and CSV sync ran first. Do not wait for decoder idle:
   // continuous reception must not starve maintenance during long sessions.
   if(ready)try{auto retention=store->retentionState();if(!retention.enabled)retentionWorking_=false;else{auto removed=store->retain(retention.limit,64,protectedThrough,retention.target);if(removed)dirty=true;bool reclaim=store->reclaimPages(32);retentionWorking_=store->retentionState().running||reclaim;}status_.retentionError.clear();retentionAt_=monotonicMs()+250;}
   catch(const std::exception& e){status_.retentionError=e.what();retentionAt_=monotonicMs()+30000;dirty=true;}
  }
  if(dirty||t-lastPublish>1000){publish(*store);lastPublish=t;}
  {std::unique_lock<std::mutex> lock(mutex_);cv_.wait_for(lock,std::chrono::milliseconds(baseline?50:100),[&]{return stopping_||queryDirty_||selectionDirty_||!writes_.empty()||!pendingSettings_.empty()||pendingDefaults_.has_value()||export_.has_value()||!moderation_.empty();});}
 }catch(const std::exception& e){std::unique_lock<std::mutex> lock(mutex_);status_.error=std::string("History: ")+e.what();auto visible=std::make_shared<Snapshot>(*snapshot_);visible->error=status_.error;visible->pendingWrites=writes_.size();visible->queued=jobs_.size();snapshot_=std::move(visible);cv_.wait_for(lock,std::chrono::seconds(1),[&]{return stopping_.load();});if(stopping_)break;}}
 if(store){try{store->setSetting("active_seconds",seconds);auto ui=store->getSetting("ui",Json::object());auto path=ui.value("csv_path",(root_/"receptions.csv").string());while(csv_.sync(*store,path,uint64_t(std::clamp(ui.value("csv_max_mb",100),1,4096))*1024*1024)){} }catch(const std::exception&){}}
}
}
