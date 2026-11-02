// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "share_card.h"
#include <future>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
 dx::ShareCard c;c.capturedUtc=1793610000;c.version=DX_VERSION;c.receiverGrid="KM69JS";c.receiverCountry="Turkey";c.modes="FT8, FT4, WSPR";c.bearingScope="Last 15 minutes / 20m / all modes";
 c.stats.countries=42;c.stats.stations=1234;c.stats.recentStations=37;c.stats.activeSeconds=18549;c.direction.degrees=313;c.direction.stations=5;
 dx::Decode d;d.identity.transmitter="JA1ABC";d.identity.grid="PM95";d.country="Japan";d.km=8764;d.snr=-19;d.mode="FT8";c.stats.farthest=8764;c.stats.farthestCall="JA1ABC";c.stats.farthestCountry="Japan";c.stats.farthestReception=d;
 auto marks=dx::shareComposition(c);auto text=[&](const std::string& find){for(const auto& m:marks)if(m.text.find(find)!=std::string::npos)return true;return false;};
 require(text("8,764 km")&&text("JA1ABC")&&text("PM95")&&text("FT8  |  -19 dB SNR")&&text("1,234")&&text("KM69JS")&&text("FT8, FT4, WSPR"),"Report evidence missing");
 require(!text("Distant signals")&&!text("KONDOR")&&text("Kondor Weak Signal Modes Decoder v"),"Requested heading/footer branding missing");
 require(text("DX STATS BY ")&&text("DX ACTIVITY BEARING:")&&text("Receiver Location:"),"Heading or bearing punctuation missing");
 bool goldCall=false,coloredModes=false;for(const auto& m:marks){if(m.accent=="MY STATION")goldCall=m.accentColor.r==239&&m.accentColor.g==193&&m.accentColor.b==115;if(m.text==c.modes)coloredModes=m.color.r==161&&m.color.g==210&&m.color.b==230;}require(goldCall&&coloredModes,"Callsign must be gold and modes must have their own color");
 c.operatorCall="My N0CALL/P";auto custom=dx::shareComposition(c);require(std::any_of(custom.begin(),custom.end(),[](const auto& m){return m.accent=="My N0CALL/P";}),"Entered callsign/spacing/case missing");c.operatorCall="MY STATION";
 auto caption=dx::shareCaption(c);require(caption.rfind("TODAY's DX STATS:\n🏆 Farthest: 8764 km, Japan, FT8, -19 dB SNR",0)==0,"Caption must start with the requested heading and farthest evidence");
 require(caption.find("Received by Kondor  https://github.com/ReLLL/kondor-weak-signal-modes-decoder")!=std::string::npos,"Caption attribution missing");
 auto later=c;later.capturedUtc+=3;later.stats.activeSeconds+=3;require(dx::sameShareContent(c,later),"Unchanged display must not redraw for seconds alone");later.stats.stations++;require(!dx::sameShareContent(c,later),"New stats must refresh");later=c;later.miles=true;require(!dx::sameShareContent(c,later),"Unit changes must refresh");later=c;later.operatorCall="My station";require(!dx::sameShareContent(c,later),"Callsign edits must refresh");later=c;later.capturedUtc+=60;require(!dx::sameShareContent(c,later),"Idle image clock must refresh on the next minute");
 const char* ordered[]={"Farthest:","42 countries heard","1,234 unique stations","Uptime:","My location: KM69JS, Turkey","DX bearing:","Modes: FT8, FT4, WSPR"};size_t position=0;for(const auto* value:ordered){auto next=caption.find(value,position);require(next!=std::string::npos,"Caption fields missing or out of order");position=next+std::strlen(value);}
 c.miles=true;require(dx::shareCaption(c).find("5446 mi")!=std::string::npos,"Caption ignored distance units");c.miles=false;
 require(dx::shareWidth==2400&&dx::shareHeight==1350,"Export must match CrispShare's X one-image preset");
 for(const auto& m:marks)if(m.kind==dx::ShareMark::Kind::Text&&(m.text==c.modes||m.text=="COUNTRIES HEARD"||m.text=="DX ACTIVITY BEARING"||m.text=="JA1ABC"))require(m.size>=38,"Key type too small for a phone preview");
 require(text("current session")&&text("Last 15 minutes"),"Scope must remain explicit");
 c.includeReceiver=false;marks=dx::shareComposition(c);require(!text("KM69JS")&&!text("Turkey")&&text("Location hidden"),"Receiver privacy toggle leaked location");
 require(dx::shareCaption(c).find("KM69JS")==std::string::npos,"Caption leaked receiver location");
 require(dx::shareServiceUrl(dx::ShareService::X,c).find("#")==std::string::npos,"X caption not URL encoded");
 c.includeReceiver=true;
 auto future=std::async(std::launch::async,[c]{return dx::renderShareCard(c);});auto image=future.get();require(image.valid(),image.error.c_str());
 const unsigned char signature[]={137,80,78,71,13,10,26,10};require(std::equal(signature,signature+8,image.png.begin()),"Not a PNG");
 for(size_t n=3;n<image.rgba.size();n+=4)require(image.rgba[n]==255,"Card must be fully opaque");
 require(image.png.size()<5*1024*1024,"Share PNG exceeds the image upload budget");
 // The embedded photograph must appear; a missing asset must not silently pass.
 auto background=dx::shareBackground();require(background.size>1000&&std::equal(signature,signature+8,background.data),"Prepared photo not embedded");
 const size_t left=(size_t(280)*dx::shareWidth+15)*4,right=(size_t(280)*dx::shareWidth+2380)*4;require(!std::equal(image.rgba.begin()+left,image.rgba.begin()+left+3,image.rgba.begin()+right),"Photographic background not rendered");
 if(argc>1){std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<const char*>(image.png.data()),std::streamsize(image.png.size()));require(bool(out),"Could not write preview");}
 c.stats={};c.direction={};c.history=true;c.oldestUtc=c.capturedUtc-86400;marks=dx::shareComposition(c);require(text("No location yet")&&text("retained history + live")&&text("History span")&&text("DX STATS BY")&&!text("TODAY'S"),"Empty/history state wrong");
 require(dx::shareCaption(c).rfind("TODAY's DX STATS:\nRetained history + live\n🏆 Farthest: no location yet",0)==0,"Empty/history caption invented evidence or hid its scope");
 require(dx::renderShareCard(c).valid(),"Empty card rendering failed");
 std::cout<<"Share evidence, privacy, scope, URL encoding, worker rendering, opaque PNG and empty/history state passed\n";
}
