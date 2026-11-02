// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <json.hpp>
#include "platform.h"
#include "table_options.h"
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <memory>
namespace dx {
// Age is presentation only; sorting/filtering always uses the original timestamp.
std::string ageText(int64_t timestamp,int64_t referenceUtc);
const char* ageLabel(int64_t timestamp,int64_t referenceUtc);
std::array<float,4> snrMapColor(double snr);
using Json=nlohmann::json;
inline constexpr int defaultFtDecodeDepth=3;
// Presentation and user-entered dates use the computer's time zone; stored epochs stay UTC.
std::string localDateTime(int64_t timestamp,bool withZone=true);
std::string durationText(int64_t seconds);
std::string historySpanText(bool enabled,int64_t oldestUtc,int64_t referenceUtc);
std::optional<int64_t> localDateTimeParse(const std::string& text,bool endOfDay=false);
std::string restoredLocalDateTime(const Json& settings,const std::string& key);
std::optional<int64_t> restoredDateTimeEpoch(const Json& settings,const std::string& key);
struct Point {double lat=0,lon=0;};
inline constexpr int64_t locationCacheSeconds=7*24*60*60;
std::optional<Point> gridPoint(const std::string& grid);
std::pair<double,double> distanceBearing(Point from,Point to);
std::vector<Point> greatCircle(Point from,Point to,int steps=128);
Point sunPosition(int64_t utc);
std::string upper(std::string s);
std::string countryName(std::string name);
std::string operatingCall(const std::string& call);
std::string callsignLookupUrl(const std::string& call,int provider);
bool matches(const std::string& value,const std::string& patterns);
struct Meaning {std::string transmitter,called,grid,type="Text";};
Meaning meaning(const std::string& message);
Meaning receptionMeaning(const std::string& mode,const std::string& message);
Meaning decodedMeaning(const std::string& mode,const std::string& message,const std::string& raw);
struct DecoderDiagnostics {std::string flags;std::optional<double> quality;bool suspect=false;};
DecoderDiagnostics decoderDiagnostics(const std::string& mode,const std::string& raw);
std::optional<Json> parseDecoderOutput(const std::string& mode,const std::string& raw);
std::string targetCall(const Meaning& identity);
enum class MessageKind {All,GeneralCall,Exchange,Other,Beacon};
MessageKind messageKind(const Meaning& identity);
struct ActivityBearing {std::optional<double> degrees;double alignment=0;int stations=0,countryEstimates=0;};
struct Decode;
struct Row;
ActivityBearing activityBearing(const std::vector<Decode>& decodes);
std::string activityBearingText(const ActivityBearing& bearing);
std::vector<Decode> farthestActivity(Point home,const std::vector<Row>& rows,int count);
std::string compassDirection(double degrees);
float arrivalOpacity(double seconds);
float cornerFlashOpacity(double seconds);
inline constexpr double arrivalHighlightSeconds=9;
double distanceDisplay(double km,bool miles);
double distanceKilometers(double value,bool miles);
const char* distanceUnit(bool miles);
std::string distanceSummary(double km,bool miles);
std::string latestHeardSummary(const Decode& decode,bool miles=false);
uint32_t callHash(const std::string& call,int bits);
struct Decode {
 int64_t utc=0,dial=0,slotMs=0; int audio=0,snr=0;
 std::optional<double> dt; std::string mode,message,raw,source="live",country,continent;
 Meaning identity; std::optional<Point> position; std::optional<double> km,bearing;
 DecoderDiagnostics diagnostics;
 int periodMs=0,submode=-1,averagedIntervals=1;std::optional<int> txPowerDbm;bool messageComplete=true;
 std::string gridSource; int64_t gridUtc=0; bool firstCall=false,firstGrid=false,firstCountry=false,distanceRecord=false;
 std::string identitySource="literal";
 std::string countryDistanceGrid; // View-only estimate; never a received grid.
 Json json() const; static Decode fromJson(const Json& j);
 std::string key() const;
};
std::string confidenceSummary(const Decode& decode);
struct IgnoreTarget {std::string value;bool message=false;bool matches(const Decode& decode)const;};
std::string distancePrefix(const Decode& decode);
std::optional<IgnoreTarget> ignoreTarget(const Decode& decode);
struct Filter {
 std::string source,search,call,called,prefix,country,continent,grid,band,mode,frequency,text,type;
 std::string excludeCall,excludeCalled,excludeCountry,excludeGrid;
 bool cqOnly=false,hasGrid=false,hasTarget=false,first=false,removeLikelyFalse=false,includeQuestionable=false;double falseToleranceKm=300;
 MessageKind messageKind=MessageKind::All;
 double minKm=0,maxKm=40080,minSnr=-100,maxDt=30;
 ColumnRanges columnRanges;
 bool hasColumnRanges()const;
 bool accepts(const Decode& d) const;
 bool accepts(const Row& row,int64_t referenceUtc)const;
 Json json() const; static Filter fromJson(const Json& j);
};
struct Query {int64_t since=0,until=INT64_MAX,afterRowId=0,firstSince=0,firstAfterId=0,referenceUtc=0;Filter filter;int sort=0;bool descending=true;bool messages=false,uniqueCalls=false,countries=false;int pathCount=3,offset=0;std::optional<std::pair<double,double>> pathSpectrum;};
Query activityQuery(const Query& view,int64_t referenceUtc);
struct MapLocation {Point point;std::string grid;int64_t utc=0;};
struct Row {Decode decode;int64_t count=1,firstHeardUtc=0,stationFirstHeardUtc=0;std::optional<MapLocation> lastKnownLocation=std::nullopt;std::optional<Point> mapPoint()const{return decode.position?decode.position:lastKnownLocation?std::optional<Point>(lastKnownLocation->point):std::nullopt;}};
struct ExchangePath{std::string transmitter,called;int64_t utc=0,fromGridUtc=0,toGridUtc=0;std::vector<Point> points;int64_t frequency=0;};
bool frequencyVisible(int64_t frequency,std::pair<double,double> spectrum);
struct Stats {int64_t countries=0,stations=0,recentStations=0,messages=0;double farthest=0,activeSeconds=0;std::string farthestCountry,farthestCall;std::vector<Decode> latest,latestCountries;std::optional<Decode> farthestReception;std::vector<std::string> heardCountries,newestCountries;bool isNewestCountry(const std::string& country)const;};
std::string bandName(int64_t hz);
struct AudioBlock {int64_t startSample=0;std::vector<int16_t> pcm;};
struct Job {uint64_t id=0;int64_t utcMs=0,dial=0,deadlineMs=0;std::string mode,source="live";std::vector<int16_t> pcm;int retry=0,periodMs=0,submode=0,decodeDepth=defaultFtDecodeDepth;uint64_t context=0;std::vector<std::shared_ptr<const AudioBlock>> audio;};
struct Preset {int64_t dial;std::string mode;int periodMs=0,submode=0;};
std::vector<Preset> presets();
std::vector<Preset> eligible(double center,double width,const std::vector<Preset>& all);
}
