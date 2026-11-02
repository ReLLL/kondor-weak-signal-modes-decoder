// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "store.h"
#include "country_summary.h"
#include "corner_feed.h"
#include "helper_client.h"
#include "alerts.h"
#include "csv_log.h"
#include "gridtracker.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
namespace dx {
struct Arrival {std::string key;int64_t observedMs=0;};
struct Snapshot {
 Stats stats;bool statsHistory=false,historyAll=false,tableMessages=false,tableCountries=false;std::vector<CountrySummary> countryTable;std::vector<Row> table,map;std::vector<Decode> details,hoverMessages;std::optional<Decode> hoverStation;
 std::vector<ExchangePath> paths;std::vector<CornerReception> cornerReceptions;std::string detailsCountry;
 std::optional<Decode> detailSelection;
 std::vector<std::string> mapHeardCountries;
 std::vector<Arrival> arrivals;int64_t newestReceptionUtc=0;ActivityBearing direction;std::vector<Decode> directionStations;
 std::string csvPath,csvError;uint64_t csvBytes=0;bool csvCatchingUp=false;
 GridTrackerState gridTracker;
 std::vector<std::string> ignoredCalls,ignoredMessages,csvPaths,savedCountries;bool moderating=false;
 Json cleanupPreview=Json::object();uint64_t cleanupPreviewId=0,completedCleanup=0;int64_t cleanupMatches=0,cleanupThroughId=0,cleanupDeleted=0,savedMessages=0;bool cleanupBusy=false;std::string cleanupError;
 int64_t tableCount=0,mapCount=0,tableExcluded=0,mapExcluded=0;
 Json settings=Json::object();uint64_t generation=0,completedModeration=0;double queryMs=0;
 Store::RetentionState retention;std::string retentionError;uint64_t historyBytes=0,historyChecks=0;std::optional<uintmax_t> sqliteBytes;
 std::string error,notice,alert,alertTone;uint64_t noticeRevision=0;int64_t alertUtc=0;bool alertSound=false;size_t pendingWrites=0,queued=0;uint64_t skipped=0,failures=0,decodedJobs=0;
 double lastDecodeMs=0;int helperPid=-1;bool decoding=false;
 std::string modeError;bool modeDecoding=false;size_t modeQueued=0;
 std::string js8Error;bool js8Decoding=false;size_t js8Queued=0;
};
class Engine {
public:
 Engine(std::filesystem::path root,std::filesystem::path helper,Point home,
  std::function<std::pair<std::string,std::string>(const std::string&)> country);
 ~Engine();void enqueue(Job job);void query(Query table,Query map,uint64_t generation);
 // Rolling clock updates must not invalidate an in-flight view request.
 void refresh(Query table,Query map);
 void select(const Decode& d,int offset=0);void settings(Json values);void exportData(Query query,std::filesystem::path path,bool csv);
 void selectCountry(std::string country,int offset=0);
 void active(bool enabled);void retry();std::shared_ptr<const Snapshot> snapshot()const;
 void alertContext(std::string bands,bool ft8,bool ft4,std::string modes="");
 void home(Point point);
 void resetStats();
 uint64_t restoreDefaults();
 void ignoreTransmitter(std::string call,bool deleteRecords=false);void ignoreMessage(std::string message,bool deleteRecords=false);void unignoreAll();
 uint64_t moderate(IgnoreTarget target,bool deleteRecords,bool ignoreFuture);
 uint64_t previewCleanup(SavedCleanup criteria);uint64_t deleteSaved(SavedCleanup criteria,int64_t throughId);
 void gridTrackerContext(GridTrackerContext context);
 void invalidateAdditional();
 void hover(const Decode& station);
private:
 void decodeLoop();void modeLoop();void js8Loop();void storeLoop();void importText(Store& store);void publish(Store& store);
 void hideIgnored(const IgnoreTarget& target);
 bool manageSaved(Store& store);
 std::filesystem::path root_,worldFile_;Helper helper_;Point home_;
 std::function<std::pair<std::string,std::string>(const std::string&)> country_;
 mutable std::mutex mutex_;std::condition_variable cv_;std::thread decoder_,writer_;std::atomic<bool> stopping_{false};std::atomic<int> decodeDepth_{defaultFtDecodeDepth};
 std::deque<Job> jobs_;std::deque<Decode> writes_;std::shared_ptr<const Snapshot> snapshot_=std::make_shared<Snapshot>();
 Helper modeHelper_;std::thread modeDecoder_;std::deque<Job> modeJobs_;uint64_t modeContext_=1;std::atomic<bool> modeCancel_{false},modeDone_{false};
 Helper js8Helper_;std::thread js8Decoder_;std::deque<Job> js8Jobs_;std::atomic<bool> js8Cancel_{false},js8Done_{false};
 Query table_,map_;uint64_t generation_=1;bool queryDirty_=true,active_=false,retry_=false,blocked_=false;
 std::optional<Decode> selection_;std::string selectionCountry_;int historyOffset_=0;bool selectionDirty_=false;
 std::optional<Decode> hovered_;
 Json pendingSettings_=Json::object();std::optional<uint64_t> pendingDefaults_;std::optional<std::tuple<Query,std::filesystem::path,bool>> export_;
 struct Moderation {IgnoreTarget target;bool deleteRecords=false,ignoreFuture=true;uint64_t id=0;};
 std::deque<Moderation> moderation_;uint64_t nextModeration_=0;
 struct CleanupRequest {SavedCleanup criteria;int64_t throughId=0;uint64_t id=0;};
 std::optional<CleanupRequest> cleanupPreview_;std::deque<CleanupRequest> cleanups_;uint64_t nextCleanup_=0;int64_t cleanupRetryAt_=0;
 Snapshot status_;int64_t retentionAt_=0,retentionCheckAt_=0;bool retentionWorking_=false;std::string allTxt_;int64_t logPosition_=0;uint64_t logInode_=0;std::string logAnchor_;
 std::optional<Point> pendingHome_;
 bool pendingStatsReset_=false;int64_t statsAfterId_=0,statsSince_=0;double sessionSeconds_=0;
 CsvLog csv_;int64_t arrivalCursor_=0,startedAt_=0,startAfterId_=0;std::atomic<bool> decoderDone_{false};
 GridTracker gridTracker_;GridTrackerContext gridTrackerContext_;CornerFeed cornerFeed_;
 std::string alertBands_,alertModes_;bool alertFT8_=true,alertFT4_=true;Alerts alerts_;int64_t lastBaseline_=0;
};
// Complete UTC slots only. A timestamp discontinuity or reset invalidates partial data.
class SlotCapture {
public:
 std::optional<Job> feed(const float* samples,size_t count,int64_t endMs,int64_t dial,const std::string& mode,int periodMs=0,int submode=0);
 void reset(){pcm_.clear();slot_=-1;nextSample_=-1;}
 double progress()const{return pcm_.size()/double(periodSamples_);}
 uint64_t discontinuities()const{return discontinuities_;}uint64_t samples()const{return samples_;}
private:
 int64_t slot_=-1,nextSample_=-1;size_t periodSamples_=180000;std::vector<int16_t> pcm_;
 std::string mode_;int64_t dial_=0;int submode_=0;
 uint64_t discontinuities_=0,samples_=0;
};
// Immutable audio blocks are shared by overlapping UTC sequences; no per-period ring.
class SequenceCapture {
public:
 std::vector<Job> feed(const float*,size_t count,int64_t endMs,int64_t dial,const std::string& mode,int selectionMs=0,int submode=0);
 void reset(){audio_.clear();nextSample_=firstSample_=-1;progress_=0;}
 double progress()const{return progress_;}
 uint64_t discontinuities()const{return discontinuities_;}uint64_t samples()const{return samples_;}
 size_t bufferedBytes()const;
private:
 std::deque<std::shared_ptr<const AudioBlock>> audio_;
 int64_t nextSample_=-1,firstSample_=-1,dial_=0;std::string mode_;int selectionMs_=-1,submode_=0;
 double progress_=0;uint64_t discontinuities_=0,samples_=0;
};
inline constexpr size_t additionalQueueLimit=32,additionalQueueBytes=96*1024*1024;
inline constexpr size_t js8QueueLimit=16,js8QueueBytes=16*1024*1024;
size_t queuedAudioBytes(const std::deque<Job>& jobs,const Job* extra=nullptr);
}
