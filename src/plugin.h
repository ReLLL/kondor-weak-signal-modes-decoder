// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "branding.h"
#include "engine.h"
#include "tiles.h"
#include "cartography.h"
#include "modes.h"
#include "status_bar.h"
#if DX_HOST_SDRSHARP
#include "capture_windows.h"
class DXHostInstance {
public:virtual ~DXHostInstance()=default;virtual void postInit(){};virtual void enable()=0;virtual void disable()=0;virtual bool isEnabled()=0;
};
#else
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/demod/ssb.h>
#include <dsp/multirate/rational_resampler.h>
using DXHostInstance=ModuleManager::Instance;
#endif
#if DX_HOST_BROWN
#include <utils/cty.h>
using DXCountryDatabase=utils::CTY;
#else
#include "country_database.h"
using DXCountryDatabase=dx::CountryDatabase;
#endif
#include <map>
class DXNotebook;
namespace dx {std::optional<std::string> chooseCsvLog(const std::string& current);bool playAlertSound(const std::string& name);std::vector<unsigned char> homeIconPixels();std::vector<unsigned char> trophyIconPixels();std::vector<unsigned char> flagIconPixels(const std::string& code);void openCallsignLookup(const std::string& call,int provider);}
#if !DX_HOST_SDRSHARP
struct DXChannel {
 DXNotebook* owner;dx::Preset preset;std::string name;dsp::channel::RxVFO* vfo=nullptr;
 dsp::demod::SSB<dsp::stereo_t> usb;dsp::sink::Handler<dsp::stereo_t> sink;
 dsp::multirate::RationalResampler<dsp::stereo_t> resampler;
 dx::SlotCapture capture;dx::SequenceCapture sequences;std::atomic<bool> accept{false};std::atomic<uint64_t> blocks{0};
 std::atomic<double> progress{0},level{-120};double lastData=0,nextRetry=0;uint64_t lastBlocks=0,reconnects=0;
 std::atomic<uint64_t> samples{0},discontinuities{0};std::atomic<int64_t> streamMs{0};
 std::atomic<bool> resetRequested{false},replay{false};
 DXChannel(DXNotebook*,dx::Preset,double center);~DXChannel();
 static void audio(dsp::stereo_t*,int,void*);
};
#endif
struct MapView {
 double lon=0,lat=0,zoom=1;bool night=true,grids=false,labels=true,counts=false,animatePaths=true,highlightCountries=true,snrColors=true,hideHome=false;
 float countryColor[4]={.32f,.72f,.40f,.16f};int recentPaths=3,base=2;
 void centerHome(dx::Point home){lon=home.lon;lat=home.lat;zoom=4;}
};
class DXNotebook:public DXHostInstance {
public:
 explicit DXNotebook(std::string name);~DXNotebook();void postInit()override{}
 void enable()override;void disable()override;bool isEnabled()override{return enabled_;}
 std::string handleDebugCommand(const std::string&,const std::string&)
#if DX_HOST_BROWN
 override
#endif
 ;
 dx::Engine& engine(){return *engine_;}
#if DX_HOST_SDRSHARP
 void renderWindowsFrame();void showWindows();bool windowsVisible()const{return open_;}
 void receiveWindowsAudio(dx::SharpAudio);
#endif
private:
 void tableTabs();void countryTable(const std::vector<dx::CountrySummary>& rows);
 int tableTab_=0,messageSort_=18,countrySort_=5;bool messageDescending_=true,countryDescending_=true,tabsApplied_=false,messageColumnsApplied_=false;
 std::vector<dx::TableColumnSetting> messageColumns_,countryColumns_;bool countryColumnsApplied_=false;
#if DX_HOST_BROWN
 using CornerBounds=ImGui::WaterFall::WaterfallDrawArgs;
#else
 struct CornerBounds {ImVec2 wfMin,wfMax;};
 ImGuiID drawHook_=0;CornerBounds cornerBounds_;
#endif
 void cornerNotifications(CornerBounds args);
 unsigned int countryFlagIcon(const std::string& country);void countryFlag(const std::string& country);unsigned int trophyTexture();
 bool cornerEnabled_=true,cornerVisible_=false,distanceMiles_=false,approximateCountryDistances_=true;int cornerSeconds_=10;ImVec2 cornerMin_,cornerMax_;std::map<std::string,std::string> flagCodes_;
 std::map<std::string,unsigned int> cornerIcons_;std::map<std::string,std::pair<std::string,int64_t>> cornerChanges_;
#if DX_HOST_BROWN
 EventHandler<ImGui::WaterFall::WaterfallDrawArgs> cornerHandler_;
#endif
 void sidebar();void draw();void openWindow();void closeWindow();void metadata();void table(const std::vector<dx::Row>&);void map(const std::vector<dx::Row>&);void stationMenu(const dx::Decode&,const std::vector<dx::Row>&);void ignoreConfirmation();void sharedFilters();void directionStat(const char*,std::optional<double>);void uptimeStat(const dx::Snapshot&);bool filterEditor(dx::Filter&,const char*);void settingsWindow();void help();void details(float height);void save();void refresh(bool changed=true);bool historyOptions(bool compact=false);bool timeWindow(const char* label,int& choice);void migration();
 std::pair<std::string,std::string> lookupCountry(const std::string& call)const;
 std::string name_;std::filesystem::path root_;std::unique_ptr<dx::Engine> engine_;dx::Point home_;DXCountryDatabase countries_;
#if !DX_HOST_BROWN
 std::string receiverGrid_,receiverCall_;bool receiverGridError_=false;
#endif
 bool enabled_=true,open_=false,present_=false,paused_=false,ft8_=true,ft4_=true,ownsGesture_=false,popup_=false,settingsLoaded_=false,detailsOpen_=false;
 dx::Json modeSettings_=dx::Json::object();bool modeSettings();bool modeEnabled(const std::string&)const;std::vector<dx::Preset> additionalPresets()const;
 int view_=0,tableTime_=2,sort_=17;bool descending_=true;float split_=.53f,detailsSplit_=.3f;
 double center_=0,rate_=0,lastRefresh_=0;uint64_t generation_=1;ImVec2 windowMin_,windowMax_;
 std::vector<std::unique_ptr<DXChannel>> channels_;std::vector<dx::Preset> presets_=dx::presets();
 dx::Filter tableFilter_;dx::Query tableQuery_,mapQuery_;std::optional<dx::Decode> selected_,detailsContext_;MapView mapView_;bool mapHomePending_=true;
 std::vector<std::vector<dx::Point>> world_;struct CountryLabel{std::string name;dx::Point point;};std::vector<CountryLabel> labels_;
 struct MapCountry{std::string name;std::vector<std::string> keys;std::vector<std::vector<std::vector<dx::Point>>> polygons;std::vector<dx::CountryMesh> offlineMeshes,onlineMeshes;dx::Point label;size_t labelPolygon=0;};std::vector<MapCountry> mapCountries_;
 std::vector<std::string> highlightedCountries_;std::vector<bool> heardCountryOutlines_;
 std::unique_ptr<dx::Tiles> tiles_;std::vector<bool> fittedLabels_;double labelScale_=-1;float labelFontSize_=0;int labelBase_=-1;
 std::vector<std::string> ignored_;char customDial_[32]="",customMode_[8]="FT8";
 std::string selectedCountry_;int detailsOffset_=0;char customSince_[32]="",customUntil_[32]="";int64_t customSinceEpoch_=0,customUntilEpoch_=0;
 std::string watchCalls_;int64_t heardAlert_=0;dx::Json alerts_=dx::Json::object();dx::Filter alertFilter_;
 std::mutex uiMutex_;
#if DX_HOST_SDRSHARP
 std::mutex captureMutex_;
#endif
 bool sortDefaultsPending_=true,settingsOpen_=false,settingsPresent_=false,settingsVisible_=false;int settingsPage_=0,columnLayout_=0;
 std::vector<dx::TableColumnSetting> tableColumns_;bool tableColumnsApplied_=false;
 ImVec2 settingsMin_,settingsMax_;std::string settingsError_;
 int clearTarget_=0;bool clearRequested_=false,clearStats_=true;int64_t tableCleared_=0,mapCleared_=0;unsigned int homeIcon_=0;
 bool clearOnStart_=true,statsHistory_=false,historyAll_=false,historyAutoTrim_=true;int historyMaxRecords_=dx::defaultHistoryLimit,historyTrimTo_=int(dx::historyCleanupTarget(dx::defaultHistoryLimit));int64_t historyTableCleared_=0,historyMapCleared_=0;int directionStations_=5,csvMaxMb_=100;char csvPath_[4096]="";
 std::string csvLogPath_;
 std::optional<dx::IgnoreTarget> ignoreRequested_;dx::IgnoreTarget ignoreTarget_;bool ignoreWaiting_=false,deletePrimary_=false,alsoAction_=false;uint64_t ignoreRequestId_=0;std::optional<dx::Decode> mapContext_;
 int cleanupKind_=0;char cleanupFrom_[32]="",cleanupTo_[32]="";std::string cleanupCountry_,cleanupValidation_;bool cleanupCached_=false;dx::SavedCleanup cleanupCriteria_;uint64_t cleanupPreviewId_=0,cleanupRequestId_=0;
 bool historyDatabaseSettings();void statusBar(const dx::Snapshot&);
 dx::StatusNotification statusNotice_;uint64_t heardNotice_=0;int64_t statusAlertUtc_=0;std::string statusError_;double statusErrorStarted_=0;
 uint64_t defaultsRequest_=0;bool defaultsReload_=false,resetWindowGeometry_=false;unsigned int resetTableLayouts_=0;
 void defaultsFooter();void loadRestoredDefaults(const dx::Snapshot&);
 bool gridTrackerEnabled_=false,gridTrackerReplay_=true;int gridTrackerPort_=2237,gridTrackerMinutes_=30;char gridTrackerAddress_[64]="127.0.0.1";std::string gridTrackerHost_="127.0.0.1";
#if !DX_HOST_SDRSHARP
 EventHandler<ImGuiContext*> drawHandler_;EventHandler<ImGui::WaterFall::InputHandlerArgs> inputHandler_;
#endif
};
