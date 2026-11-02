// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include "saved_cleanup.h"
#include "location_check.h"
#include <sqlite3.h>
#include <filesystem>
#include <functional>
#include <map>
#include <cmath>
namespace dx {
constexpr int defaultHistoryLimit=100000;
constexpr int64_t historyCheckIntervalMs=30*60*1000;
constexpr int64_t historyCleanupTarget(int64_t limit,int64_t requested=0){return requested>0?std::clamp(requested,int64_t(1),std::max<int64_t>(1,limit-1)):limit*95/100;}
class Store {
public:
 explicit Store(const std::filesystem::path& file);~Store();
 Store(const Store&)=delete;Store& operator=(const Store&)=delete;
 bool insert(Decode& d,Point home,const std::function<std::pair<std::string,std::string>(const std::string&)>& country={});
 std::vector<Row> rows(const Query& query,int offset=0,int limit=10000,int64_t* countOnly=nullptr);
 void mapLocations(std::vector<Row>& rows);
 void countryDistances(bool enabled,Point home){countryDistances_=enabled;distanceHome_=home;}
 void projectDistance(Decode& decode);
 bool loadCountryAreas(const std::filesystem::path& file){invalidateSession();return locationCheck_.load(file);}
 std::vector<Decode> history(const std::string& call,int64_t dial,const std::string& mode,int offset=0);
 std::vector<Decode> uniqueMessages(const Decode& station,const Query& view);
 Stats stats(int64_t now,const Filter& filter={});int64_t importBridge(const std::filesystem::path&,Point,const std::function<std::pair<std::string,std::string>(const std::string&)>&);
 Stats sessionStats(int64_t now,int64_t afterRowId,int64_t since,const Filter& filter={});
 bool passesLocationFilter(const Decode& d,const Filter& filter){return (filter.includeQuestionable||!d.diagnostics.suspect)&&(!filter.removeLikelyFalse||!locationCheck_.mismatch(d.country,d.identity.grid,d.identity.transmitter,filter.falseToleranceKm));}
 Json getSetting(const std::string& key,const Json& fallback);void setSetting(const std::string& key,const Json& value);
 void exportRows(const Query& query,const std::filesystem::path& path,bool csv);
 void reclassifyCountries(const std::function<std::pair<std::string,std::string>(const std::string&)>& country);
 void rebaseDistances(Point home);
 std::vector<ExchangePath> exchangePaths(const Query& query);
 std::vector<std::pair<int64_t,Decode>> logRows(int64_t after,int limit=256);
 int64_t lastReceptionId();
 int64_t ignoreTransmitter(const std::string& call,bool deleteRecords=false);
 int64_t ignoreMessage(const std::string& message,bool deleteRecords=false);bool ignoredMessage(const std::string& message);std::vector<std::string> ignoredMessages();bool ignoredReception(const Decode& decode);
 int64_t deleteTransmitter(const std::string& call,bool ignoreFuture=false);
 int64_t deleteMessage(const std::string& message,bool ignoreFuture=false);
 int64_t moderate(const IgnoreTarget& target,bool deleteRecords,bool ignoreFuture);
 int64_t countCleanup(const SavedCleanup& cleanup,int64_t throughId);
 int64_t deleteSaved(const SavedCleanup& cleanup,int64_t throughId);
 std::vector<std::string> savedCountries();
 int64_t savedMessageCount();
 void compact();
 struct RetentionState {int64_t firstUtc=0,lastUtc=0,deleted=0,limit=defaultHistoryLimit,target=historyCleanupTarget(defaultHistoryLimit);bool enabled=true,running=false;};
 RetentionState retentionState();
 // One bounded transaction. The caller must drain reception/CSV work first.
 int64_t retain(int64_t limit,int batch=128,int64_t syncedThrough=INT64_MAX,int64_t trimTo=0);
 bool reclaimPages(int pages=32);
 void unignoreAll();std::vector<std::string> ignoredCalls();bool ignored(const std::string& call);
private:
 LocationCheck locationCheck_;
 bool countryDistances_=false;Point distanceHome_{NAN,NAN};
 std::optional<LocationCheck::CountryDistance> countryDistance(const std::string& country);
 std::string distanceSql(bool payload=false)const;
 void ensureReceptionIds();void ensureEntities();void rebuildSummaries(bool stations=true);
 struct StatsCache {
  int64_t after=-1,since=0,cursor=0,dataVersion=-1,farthestId=0;
  bool filtered=false,receivedOnly=false,includeQuestionable=false;double tolerance=300;
  Stats totals;std::map<std::string,std::pair<int64_t,Decode>> calls,countries;
  std::map<std::string,int64_t> countryFirst;
 };
 Stats scopedStats(int64_t now,int64_t after,int64_t since,const Filter&,StatsCache&,bool receivedOnly);
 void expireStats(StatsCache&,const std::vector<std::pair<int64_t,Decode>>&);
 void invalidateSession(){sessionCache_.after=historyCache_.after=-1;}
 void expireSession(const std::vector<std::pair<int64_t,Decode>>& expired);
 void exec(const std::string& sql);sqlite3* db_=nullptr;
 StatsCache sessionCache_,historyCache_;
};
}
