// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "share_card.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <ctime>
namespace dx {
namespace {
constexpr ShareColor paper{244,247,246},muted{155,181,193},gold{239,193,115},lineColor{47,76,94},ink{17,39,56};
std::string number(int64_t value){auto s=std::to_string(std::max<int64_t>(0,value));for(int i=int(s.size())-3;i>0;i-=3)s.insert(size_t(i),",");return s;}
std::string escaped(const std::string& text){std::ostringstream out;out<<std::uppercase<<std::hex;for(unsigned char c:text)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out<<c;else out<<'%'<<std::setw(2)<<std::setfill('0')<<int(c);return out.str();}
}
std::vector<ShareMark> shareComposition(const ShareCard& c){
 std::vector<ShareMark> marks;
 auto rect=[&](float x,float y,float w,float h,ShareColor color){marks.push_back({ShareMark::Kind::Rectangle,x,y,w,h,0,color});};
 auto line=[&](float x,float y,float x2,float y2,float width,ShareColor color){marks.push_back({ShareMark::Kind::Line,x,y,x2,y2,width,color});};
 auto ellipse=[&](float x,float y,float w,float h,float width,ShareColor color){marks.push_back({ShareMark::Kind::Ellipse,x,y,w,h,width,color});};
 auto text=[&](float x,float y,float width,float size,const std::string& value,ShareColor color=paper,bool bold=false,bool display=false){marks.push_back({ShareMark::Kind::Text,x,y,width,size*1.4f,size,color,value,bold,display});};
 rect(0,0,shareWidth,shareHeight,ink);
 marks.push_back({ShareMark::Kind::Background,0,0,shareWidth,shareHeight});
 // The prepared photo is embedded. Dark scrims protect contrast on small screens.
 rect(0,0,shareWidth,shareHeight,{4,13,23,105});
 rect(76,294,1765,320,{5,17,28,125});
 rect(0,626,shareWidth,724,{5,17,28,145});
 text(100,48,2200,36,localDateTime(c.capturedUtc),muted);
 // History/session scope is not a claim that all retained receptions are today's.
 auto captured=static_cast<time_t>(c.capturedUtc);std::tm local{};
#if defined(_WIN32)
 localtime_s(&local,&captured);
#else
 localtime_r(&captured,&local);
#endif
 local.tm_hour=local.tm_min=local.tm_sec=0;local.tm_isdst=-1;
 const bool today=!c.history&&c.stats.activeSeconds<=std::difftime(captured,std::mktime(&local));
 text(100,112,2200,62,today?"TODAY'S DX STATS BY ":"DX STATS BY ",paper,true,true);
 marks.back().accent=c.operatorCall.empty()?"MY STATION":c.operatorCall;
 text(102,207,2150,52,c.modes.empty()?"No modes enabled":c.modes,{161,210,230},true);
 marks.push_back({ShareMark::Kind::Trophy,100,318,46,46,0,gold});
 text(166,316,1500,42,"FARTHEST STATION HEARD",gold,true);
 const auto& s=c.stats;
 const bool known=s.farthest>0&&s.farthestReception.has_value();
 const auto distance=known?number(int64_t(std::llround(distanceDisplay(s.farthest,c.miles))))+" "+distanceUnit(c.miles):"No location yet";
 text(98,379,860,known?154:63,distance,paper,true,true);
 text(1000,369,1240,60,known?s.farthestCall:"Listening for distant signals",paper,true);
 text(1004,447,1240,76,known?s.farthestCountry:"Waiting for a received location",paper,true);
 const auto grid=known?s.farthestReception->identity.grid:std::string{};
 const auto mode=known?s.farthestReception->mode:std::string{};
 std::string evidence=known?(mode.empty()?std::string("Mode unknown"):mode)+"  |  "+(s.farthestReception->snr>=0?"+":"")+std::to_string(s.farthestReception->snr)+" dB SNR  |  "+(grid.empty()?std::string("Grid unknown"):grid):"Distances are from the receiver.";
 if(known&&s.farthestReception->gridSource=="cached")evidence+="*";
 text(1004,553,1240,42,evidence,paper);
 text(100,668,590,42,"COUNTRIES HEARD",muted,true);text(98,723,590,114,number(s.countries),paper,true,true);
 text(760,668,650,42,"UNIQUE STATIONS",muted,true);text(758,723,650,114,number(s.stations),paper,true,true);
 text(1510,668,790,42,"UPTIME",muted,true);text(1508,744,790,70,durationText(int64_t(s.activeSeconds)),paper,true);
 text(100,900,2200,48,c.includeReceiver?"Receiver Location:  "+(c.receiverGrid.empty()?std::string("Grid not set"):c.receiverGrid)+(c.receiverCountry.empty()?"":"  /  "+c.receiverCountry):"Receiver Location:  Location hidden",paper,true);
 text(100,1005,630,38,"DX ACTIVITY BEARING:",muted,true);
 text(735,995,590,60,activityBearingText(c.direction),paper,true);
 // A geographic compass, not a claim of measured direction of arrival.
 const float cx=1370,cy=1032,r=40;ellipse(cx-r,cy-r,r*2,r*2,2,lineColor);
 if(c.direction.degrees){double a=*c.direction.degrees*3.141592653589793/180;line(cx,cy,cx+float(std::sin(a))*31,cy-float(std::cos(a))*31,7,{222,112,99});line(cx,cy,cx-float(std::sin(a))*27,cy+float(std::cos(a))*27,7,paper);}
 text(1510,1006,790,42,number(s.recentStations)+" stations / last 5 min",paper,true);
 if(c.history&&c.oldestUtc>0)text(1510,829,790,32,"History span "+durationText(std::max<int64_t>(0,c.capturedUtc-c.oldestUtc)),gold);
 text(100,1112,2200,29,(c.history?"Stats: retained history + live":"Stats: current session")+std::string("  |  Bearing: ")+c.bearingScope,muted);
 line(100,1186,2300,1186,1,lineColor);
 text(100,1211,1080,29,"github.com/ReLLL/kondor-weak-signal-modes-decoder",muted);
 text(1230,1207,1070,37,"Kondor Weak Signal Modes Decoder v"+c.version,gold,true);
 text(100,1281,2200,24,"Receive only. Grid distances are approximate; bearings describe geographic activity, not measured propagation paths. * Last known grid.",muted);
 return marks;
}
std::string shareCaption(const ShareCard& c){
 const auto& s=c.stats;std::string text="TODAY's DX STATS:\n";if(c.history)text+="Retained history + live\n";text+="🏆 Farthest: ";
 if(s.farthest>0&&s.farthestReception){const auto& d=*s.farthestReception;text+=distanceSummary(s.farthest,c.miles)+", "+s.farthestCountry+", "+(d.mode.empty()?"mode unknown":d.mode)+", "+(d.snr>=0?"+":"")+std::to_string(d.snr)+" dB SNR - "+s.farthestCall;if(!d.identity.grid.empty())text+=" ("+d.identity.grid+")";}
 else text+="no location yet";
 text+="\n"+number(s.countries)+" countries heard, "+number(s.stations)+" unique stations\nUptime: "+durationText(int64_t(s.activeSeconds));
 text+="\nMy location: "+(c.includeReceiver?(c.receiverGrid.empty()?std::string("grid not set"):c.receiverGrid)+(c.receiverCountry.empty()?"":", "+c.receiverCountry):std::string("location hidden"));
 text+="\nDX bearing: "+activityBearingText(c.direction)+" | "+number(s.recentStations)+" stations/5 min\nModes: "+(c.modes.empty()?std::string("none"):c.modes);
 text+="\nReceived by Kondor  https://github.com/ReLLL/kondor-weak-signal-modes-decoder";return text;
}
bool sameShareContent(const ShareCard& a,const ShareCard& b){
 return a.capturedUtc/60==b.capturedUtc/60&&a.oldestUtc==b.oldestUtc&&a.history==b.history&&a.operatorCall==b.operatorCall&&a.version==b.version&&a.bearingScope==b.bearingScope&&a.miles==b.miles&&a.includeReceiver==b.includeReceiver&&shareCaption(a)==shareCaption(b);
}
std::string shareServiceUrl(ShareService service,const ShareCard& c){
 if(service==ShareService::X)return "https://x.com/intent/post?text="+escaped(shareCaption(c));
 if(service==ShareService::Facebook)return "https://www.facebook.com/";
 return "https://www.instagram.com/";
}
}
