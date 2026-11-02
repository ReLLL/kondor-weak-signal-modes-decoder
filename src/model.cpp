// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>
#include <ctime>
#include <fnmatch.h>
namespace dx {
bool frequencyVisible(int64_t frequency,std::pair<double,double> spectrum){return frequency>0&&std::isfinite(spectrum.first)&&std::isfinite(spectrum.second)&&spectrum.first<=spectrum.second&&frequency>=spectrum.first&&frequency<=spectrum.second;}
const char* ageLabel(int64_t timestamp,int64_t referenceUtc){return timestamp>0&&referenceUtc>timestamp&&referenceUtc-timestamp>600?"Date":"Age";}
std::string ageText(int64_t timestamp,int64_t referenceUtc){
 if(timestamp<=0)return "--";
 if(referenceUtc<=timestamp)return "0s ago";
 const auto seconds=referenceUtc-timestamp;
 if(seconds<=600)return std::to_string(seconds)+"s ago";
 return localDateTime(timestamp);
}
std::array<float,4> snrMapColor(double snr){
 if(!std::isfinite(snr))return {.62f,.67f,.70f,1};
 const std::array<float,4> red{.95f,.25f,.20f,1},yellow{1,.82f,.24f,1},green{.28f,.85f,.40f,1};
 double level=std::clamp((snr+30)/40,0.,1.);const auto& from=level<=.5?red:yellow;const auto& to=level<=.5?yellow:green;
 float mix=float(level<=.5?level*2:(level-.5)*2);std::array<float,4> color{};
 for(size_t i=0;i<color.size();i++)color[i]=from[i]+(to[i]-from[i])*mix;return color;
}
constexpr double rad=3.14159265358979323846/180.;
float cornerFlashOpacity(double seconds){return std::isfinite(seconds)&&seconds>0&&seconds<2?float(std::sin(seconds*3.14159265358979323846/2)):0.f;}
std::string upper(std::string s){for(char& c:s)c=char(std::toupper(static_cast<unsigned char>(c)));return s;}
std::optional<Point> gridPoint(const std::string& input){
 auto g=upper(input);if(g=="RR73" || (g.size()!=4&&g.size()!=6&&g.size()!=8))return {};
 double lon=-180,lat=-90,w=20,h=10;
 for(size_t i=0;i<g.size();i+=2){int x,y;
  if(i==0){x=g[i]-'A';y=g[i+1]-'A';if(x<0||x>=18||y<0||y>=18)return {};}
  else if(i==4){x=g[i]-'A';y=g[i+1]-'A';if(x<0||x>=24||y<0||y>=24)return {};w/=24;h/=24;}
  else{x=g[i]-'0';y=g[i+1]-'0';if(x<0||x>=10||y<0||y>=10)return {};w/=10;h/=10;}
  lon+=x*w;lat+=y*h;
 }return Point{lat+h/2,lon+w/2};
}
std::pair<double,double> distanceBearing(Point a,Point b){
 double p=a.lat*rad,q=b.lat*rad,d=(b.lon-a.lon)*rad;
 double h=std::pow(std::sin((q-p)/2),2)+std::cos(p)*std::cos(q)*std::pow(std::sin(d/2),2);
 double km=6371.0088*2*std::asin(std::sqrt(std::clamp(h,0.,1.)));
 double bearing=std::atan2(std::sin(d)*std::cos(q),std::cos(p)*std::sin(q)-std::sin(p)*std::cos(q)*std::cos(d))/rad;
 return {km,std::fmod(bearing+360.,360.)};
}
std::vector<Point> greatCircle(Point a,Point b,int steps){
 auto vec=[](Point p){double lat=p.lat*rad,lon=p.lon*rad;return std::array<double,3>{std::cos(lat)*std::cos(lon),std::cos(lat)*std::sin(lon),std::sin(lat)};};
 auto x=vec(a),y=vec(b);double dot=0;for(int i=0;i<3;i++)dot+=x[i]*y[i];
 double angle=std::acos(std::clamp(dot,-1.,1.));std::array<double,3> tangent{};
 double norm=0;for(int i=0;i<3;i++){tangent[i]=y[i]-dot*x[i];norm+=tangent[i]*tangent[i];}
 norm=std::sqrt(norm);if(norm<1e-10){tangent={-x[1],x[0],0};norm=std::hypot(tangent[0],tangent[1]);if(norm<1e-10){tangent={1,0,0};norm=1;}}
 for(double& v:tangent)v/=norm;
 std::vector<Point> out;for(int i=0;i<=steps;i++){double t=angle*i/steps;std::array<double,3> v{};for(int k=0;k<3;k++)v[k]=x[k]*std::cos(t)+tangent[k]*std::sin(t);out.push_back({std::asin(std::clamp(v[2],-1.,1.))/rad,std::atan2(v[1],v[0])/rad});}return out;
}
Point sunPosition(int64_t utc){
 double jd=2440587.5+utc/86400.,n=jd-2451545.,L=std::fmod(280.46+.9856474*n,360.),g=(357.528+.9856003*n)*rad;
 double lambda=(L+1.915*std::sin(g)+.020*std::sin(2*g))*rad,e=(23.439-.0000004*n)*rad;
 double dec=std::asin(std::sin(e)*std::sin(lambda))/rad;
 double ra=std::atan2(std::cos(e)*std::sin(lambda),std::cos(lambda))/rad;
 double gmst=280.46061837+360.98564736629*n;
 double lon=std::remainder(ra-gmst,360.);return {dec,lon};
}
bool matches(const std::string& value,const std::string& patterns){
 if(patterns.empty())return true;std::stringstream in(upper(patterns));std::string p,v=upper(value);
 while(std::getline(in,p,',')){auto first=p.find_first_not_of(' '),last=p.find_last_not_of(' ');if(first==std::string::npos)continue;p=p.substr(first,last-first+1);if(fnmatch(p.c_str(),v.c_str(),0)==0)return true;}return false;
}
std::string callsignLookupUrl(const std::string& input,int provider){
 auto call=upper(input);if(call.size()<3||call.size()>32||call.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/")!=std::string::npos||call.find_first_of("0123456789")==std::string::npos||call.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")==std::string::npos)return {};
 auto encode=[](const std::string& text){std::string out;const char* hex="0123456789ABCDEF";for(unsigned char c:text){if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);else{out+='%';out+=hex[c>>4];out+=hex[c&15];}}return out;};
 if(provider==0)return "https://www.google.com/search?q="+encode(call+" call sign");
 if(provider==1)return "https://www.qrz.com/db/"+encode(call);
 if(provider==2)return "https://www.hamqth.com/"+encode(call);return {};
}
Meaning meaning(const std::string& message){
 Meaning m;std::stringstream in(upper(message));std::vector<std::string> w;std::string s;while(in>>s)w.push_back(s);
 static const std::regex callPattern("[A-Z0-9/]+"),reportPattern("R?[+-][0-9]{2}"),exchangePattern("[0-9]{6}"),contestPattern("[1-9][0-9]?[A-H]|5[0-9]{2}"),sectionPattern("[A-Z]{2,3}|[0-9]{1,4}");
 auto call=[](const std::string& c){if(c.size()>2&&c.front()=='<'&&c.back()=='>')return true;return c!="RR73"&&c.size()>=3&&c.size()<=24&&std::regex_match(c,callPattern)&&c.find_first_of("0123456789")!=std::string::npos&&c.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::string::npos;};
 if(!w.empty()&&w.front()=="TU;")w.erase(w.begin()); // RTTY contest acknowledgement prefix.
 if(w.size()<2)return m;
 if(w[0]=="CQ"||w[0]=="QRZ"||w[0]=="DE"){
  m.type=w[0];m.called=w[0];size_t index=1;if(w.size()>2&&!call(w[1]))index=2;
  if(index<w.size()&&call(w[index]))m.transmitter=w[index];
 }else if(w.size()>=2&&call(w[0])&&call(w[1])){m.called=w[0];m.transmitter=w[1];}
 if(!m.transmitter.empty()&&m.called!="CQ"&&m.called!="QRZ"&&m.called!="DE"){
  const auto& last=w.back();bool tail=w.size()==2||(w.size()==3&&(gridPoint(last)||last=="RRR"||last=="RR73"||last=="73"||std::regex_match(last,reportPattern)));
  tail|=w.size()==4&&((w[2]=="R"&&gridPoint(last))||(std::regex_match(w[2],exchangePattern)&&last.size()==6&&gridPoint(last))||(std::regex_match(w[2],contestPattern)&&std::regex_match(last,sectionPattern)));
  tail|=w.size()==5&&w[2]=="R"&&((std::regex_match(w[3],exchangePattern)&&last.size()==6&&bool(gridPoint(last)))||(std::regex_match(w[3],contestPattern)&&std::regex_match(last,sectionPattern)));
  if(!tail)return {};
 }
 if(!m.transmitter.empty()&&w.size()>=3){auto last=w.back();if(gridPoint(last)){m.grid=last;m.type=m.type=="Text"?"Grid":m.type;}
  else if(last=="RRR"||last=="RR73"||last=="73")m.type=last;
  else if(last.front()=='R'&&std::regex_match(last,reportPattern))m.type="R-report";
  else if(std::regex_match(last,reportPattern))m.type="Report";
 }for(auto* value:{&m.transmitter,&m.called})if(value->size()>2&&value->front()=='<'&&value->back()=='>'){auto inner=value->substr(1,value->size()-2);if(std::regex_match(inner,callPattern)&&inner.find_first_of("0123456789")!=std::string::npos&&inner.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::string::npos)*value=std::move(inner);}return m;
}
Meaning receptionMeaning(const std::string& mode,const std::string& message){
 if(mode=="JS8"){
  Meaning result;auto colon=message.find(':');if(colon==std::string::npos||colon>24)return result;auto sender=upper(message.substr(0,colon));
  auto call=[](const std::string& value){return value.size()>=3&&value.size()<=24&&std::regex_match(value,std::regex("[A-Z0-9/]+"))&&value.find_first_of("0123456789")!=std::string::npos&&value.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::string::npos;};
  if(!call(sender))return result;result.transmitter=sender;std::istringstream input(upper(message.substr(colon+1)));std::vector<std::string> words;std::string word;while(input>>word)words.push_back(word);
  if(words.empty())return result;
  if(words[0]=="@HB"&&(words.size()>1&&words[1]=="HEARTBEAT")){result.type="Heartbeat";result.called="@HB";}
  else if(words[0]=="@ALLCALL"&&words.size()>1&&words[1].rfind("CQ",0)==0){result.type="CQ";result.called="CQ";}
  else if(call(words[0])||(words[0].front()=='@'&&std::regex_match(words[0],std::regex("@[A-Z0-9/]{1,12}")))){result.called=words[0];result.type="Directed";}
  bool locationAnnouncement=result.type=="Heartbeat"&&words.size()==3;
  if(result.type=="CQ"&&words.size()>=3&&words.size()<=5){std::string cq;for(size_t i=1;i+1<words.size();i++){if(!cq.empty())cq+=' ';cq+=words[i];}for(const char* form:{"CQ CQ CQ","CQ DX","CQ QRP","CQ CONTEST","CQ FIELD","CQ FD","CQ CQ","CQ"})if(cq==form)locationAnnouncement=true;}
  if(locationAnnouncement&&gridPoint(words.back()))result.grid=words.back();return result;
 }
 if(mode!="WSPR"&&mode!="FST4W")return meaning(message);
 Meaning result;result.type="Beacon";std::istringstream input(upper(message));std::string call,grid,power,extra;input>>call>>grid>>power;
 if(call.empty()||call.size()>24)return result;
 if(std::regex_match(call,std::regex("<[A-Z0-9/]+>"))||(std::regex_match(call,std::regex("[A-Z0-9/]+"))&&call.find_first_of("0123456789")!=std::string::npos&&call.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::string::npos))result.transmitter=call;
 if(gridPoint(grid))result.grid=grid;
 return result;
}
uint32_t callHash(const std::string& input,int bits){
 // MSHV / WSJT-X 77-bit message callsign hash (upstream PackUnpackMsg77).
 if(bits!=10&&bits!=12&&bits!=22)return 0;std::string call=upper(input),alphabet=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";call.resize(11,' ');uint64_t value=0;
 for(size_t i=0;i<11;i++){auto index=alphabet.find(call[i]);if(index==std::string::npos)return 0;value=38*value+index;}return uint32_t((value*47055833459ULL)>>(64-bits));
}
std::string targetCall(const Meaning& identity){return identity.called=="CQ"||identity.called=="QRZ"||identity.called=="DE"?"":identity.called;}
MessageKind messageKind(const Meaning& identity){
 if(identity.type=="Beacon"||identity.type=="Heartbeat")return MessageKind::Beacon;
 if(identity.called=="CQ"||identity.called=="QRZ")return MessageKind::GeneralCall;
 if(!identity.transmitter.empty()&&!identity.called.empty()&&identity.called!="DE"&&identity.called.front()!='@')return MessageKind::Exchange;
 return MessageKind::Other;
}
ActivityBearing activityBearing(const std::vector<Decode>& decodes){
 ActivityBearing result;double x=0,y=0;for(const auto& d:decodes)if(d.bearing&&d.km&&*d.km>=1&&std::isfinite(*d.km)&&std::isfinite(*d.bearing)){x+=std::cos(*d.bearing*rad);y+=std::sin(*d.bearing*rad);++result.stations;if(!d.countryDistanceGrid.empty())++result.countryEstimates;}
 if(!result.stations)return result;result.alignment=std::hypot(x,y)/result.stations;
 if(result.alignment>=.5)result.degrees=std::fmod(std::atan2(y,x)/rad+360.,360.);return result;
}
std::string activityBearingText(const ActivityBearing& bearing){
 if(!bearing.degrees)return bearing.stations?"Mixed directions":"No matching stations";
 return std::to_string(int(std::fmod(std::round(*bearing.degrees),360.)))+"° | "+compassDirection(*bearing.degrees);
}
std::string compassDirection(double degrees){const char* names[]={"North","Northeast","East","Southeast","South","Southwest","West","Northwest"};return names[int(std::floor((std::fmod(degrees+360.,360.)+22.5)/45.))%8];}
std::vector<Decode> farthestActivity(Point home,const std::vector<Row>& rows,int count){
 // Country estimates are transient activity inputs, never station positions or reception evidence.
 std::vector<Decode> stations;for(const auto& row:rows){auto d=row.decode;auto point=d.position?d.position:gridPoint(d.countryDistanceGrid);if(!point||d.identity.transmitter.empty())continue;auto path=distanceBearing(home,*point);if(!std::isfinite(path.first)||!std::isfinite(path.second)||path.first<1)continue;d.km=path.first;d.bearing=path.second;stations.push_back(std::move(d));}
 std::stable_sort(stations.begin(),stations.end(),[](const Decode& a,const Decode& b){return *a.km>*b.km;});if(stations.size()>size_t(std::max(0,count)))stations.resize(size_t(std::max(0,count)));return stations;
}
Query activityQuery(const Query& view,int64_t referenceUtc){
 auto query=view;query.uniqueCalls=true;query.messages=query.countries=false;query.sort=3;query.descending=true;query.offset=0;
 query.until=std::min(query.until,referenceUtc);query.referenceUtc=referenceUtc;
 return query;
}
float arrivalOpacity(double seconds){if(seconds<0||seconds>=arrivalHighlightSeconds)return 0;return float(seconds<.45?seconds/.45:std::pow(1-(seconds-.45)/(arrivalHighlightSeconds-.45),2));}
double distanceDisplay(double km,bool miles){return miles?km/1.609344:km;}
double distanceKilometers(double value,bool miles){return miles?value*1.609344:value;}
const char* distanceUnit(bool miles){return miles?"mi":"km";}
std::string distanceSummary(double km,bool miles){return std::to_string(int(std::round(distanceDisplay(km,miles))))+" "+distanceUnit(miles);}
std::string distancePrefix(const Decode& d){return !d.countryDistanceGrid.empty()?"[C] ":d.gridSource=="cached"?"* ":"";}
std::string latestHeardSummary(const Decode& d,bool miles){return (d.country.empty()?"Unknown":d.country)+std::string(" | ")+(d.km?distancePrefix(d)+distanceSummary(*d.km,miles):"distance unknown")+" | "+(d.snr>=0?"+":"")+std::to_string(d.snr)+" dB SNR";}
std::string bandName(int64_t f){
 for(auto [low,high,name]:std::vector<std::tuple<int64_t,int64_t,std::string>>{{135700,137800,"2200m"},{472000,479000,"630m"},{1800000,2000000,"160m"},{3500000,4000000,"80m"},{5000000,5500000,"60m"},{7000000,7300000,"40m"},{10000000,10200000,"30m"},{14000000,14400000,"20m"},{18000000,18200000,"17m"},{21000000,21500000,"15m"},{24800000,25000000,"12m"},{28000000,30000000,"10m"},{50000000,54000000,"6m"},{144000000,148000000,"2m"}})if(f>=low&&f<=high)return name;
 return "Other";
}
std::string countryName(std::string name){return name=="Fed. Rep. of Germany"?"Germany":name;}
bool Stats::isNewestCountry(const std::string& country)const{return !country.empty()&&std::find(newestCountries.begin(),newestCountries.end(),country)!=newestCountries.end();}
Json Decode::json()const{
 Json j={{"timestamp",utc},{"slot_ms",slotMs},{"frequency",dial},{"mode",mode},{"audio_hz",audio},{"snr",snr},{"dt",dt?Json(*dt):Json(nullptr)},{"message",message},{"raw",raw},{"source",source},{"country",countryName(country)},{"continent",continent},{"transmitter",identity.transmitter},{"called",identity.called},{"grid",identity.grid},{"type",identity.type},{"identity_source",identitySource},{"grid_source",gridSource},{"grid_utc",gridUtc},{"km",km?Json(*km):Json(nullptr)},{"bearing",bearing?Json(*bearing):Json(nullptr)},{"first_call",firstCall},{"first_grid",firstGrid},{"first_country",firstCountry},{"distance_record",distanceRecord}};
 if(!diagnostics.flags.empty())j["decoder_flags"]=diagnostics.flags;if(diagnostics.quality)j["decoder_quality"]=*diagnostics.quality;if(diagnostics.suspect)j["decoder_suspect"]=true;
 if(periodMs)j["period_ms"]=periodMs;if(submode>=0)j["submode"]=submode;if(averagedIntervals>1)j["averaged_intervals"]=averagedIntervals;if(txPowerDbm)j["tx_power_dbm"]=*txPowerDbm;if(!messageComplete)j["message_complete"]=false;if(!countryDistanceGrid.empty()){j["distance_source"]="country";j["distance_estimate_grid"]=countryDistanceGrid;}return j;
}
Decode Decode::fromJson(const Json& j){
 Decode d;d.utc=j.at("timestamp");d.slotMs=j.value("slot_ms",d.utc*1000);d.dial=j.at("frequency");d.mode=j.at("mode");d.snr=j.at("snr");d.audio=j.at("audio_hz");d.message=j.at("message");d.raw=j.value("raw","");d.source=j.value("source","legacy");
 if(j.contains("dt")&&!j["dt"].is_null()&&d.source!="legacy")d.dt=j["dt"].get<double>();auto parsedIdentity=decodedMeaning(d.mode,d.message,d.raw);d.identity=parsedIdentity;d.diagnostics=decoderDiagnostics(d.mode,d.raw);d.diagnostics.suspect=d.diagnostics.suspect||j.value("decoder_suspect",false);if(j.contains("decoder_flags"))d.diagnostics.flags=j.value("decoder_flags",d.diagnostics.flags);if(j.contains("decoder_quality")&&j["decoder_quality"].is_number()){auto q=j["decoder_quality"].get<double>();if(std::isfinite(q)&&q>=0&&q<=1)d.diagnostics.quality=q;}d.identitySource=j.value("identity_source","literal");
 if(j.contains("transmitter"))d.identity.transmitter=j["transmitter"];if(j.contains("called"))d.identity.called=j["called"];d.country=countryName(j.value("country",""));d.continent=j.value("continent","");d.gridSource=j.value("grid_source","");d.gridUtc=j.value("grid_utc",int64_t(0));
 if(j.contains("grid"))d.identity.grid=j["grid"];d.position=gridPoint(d.identity.grid);if(j.contains("km")&&!j["km"].is_null())d.km=j["km"].get<double>();if(j.contains("bearing")&&!j["bearing"].is_null())d.bearing=j["bearing"].get<double>();
 if((d.mode=="FT8"||d.mode=="FT4")&&d.raw.rfind(d.mode+"_OUT\t",0)==0&&parsedIdentity.transmitter.empty()){d.identity={};d.country.clear();d.continent.clear();d.position.reset();d.km.reset();d.bearing.reset();d.gridSource.clear();}
 d.firstCall=j.value("first_call",false);d.firstGrid=j.value("first_grid",false);d.firstCountry=j.value("first_country",false);d.distanceRecord=j.value("distance_record",false);
 d.periodMs=j.value("period_ms",0);d.submode=j.value("submode",-1);d.averagedIntervals=j.value("averaged_intervals",1);d.messageComplete=j.value("message_complete",true);if(j.value("distance_source",std::string{})=="country")d.countryDistanceGrid=j.value("distance_estimate_grid",std::string{});
 if(d.mode=="WSPR"||d.mode=="FST4W"){
  std::istringstream words(upper(d.message));std::string word,last;while(words>>word)last=word;
  if(std::regex_match(last,std::regex("[0-9]{1,2}"))&&std::stoi(last)<=60)d.txPowerDbm=std::stoi(last);
  if(!d.identity.transmitter.empty()&&d.identity.transmitter.front()=='<')d.identitySource="decoder-hash";
 }
 return d;
}
std::string Decode::key()const{return Json::array({utc,dial,mode,snr,audio,message}).dump();}
bool IgnoreTarget::matches(const Decode& d)const{return value==upper(message?d.message:d.identity.transmitter);}
std::optional<IgnoreTarget> ignoreTarget(const Decode& d){IgnoreTarget target{upper(d.identity.transmitter.empty()?d.message:d.identity.transmitter),d.identity.transmitter.empty()};if(target.value.empty()||target.value.find('\0')!=std::string::npos)return {};return target;}
bool Filter::accepts(const Decode& d)const{
 const auto& i=d.identity;
 if(!includeQuestionable&&d.diagnostics.suspect)return false;
 if(!columnRanges[size_t(ColumnMetric::Confidence)].accepts(d.diagnostics.quality))return false;
 if((!excludeCall.empty()&&matches(i.transmitter,excludeCall))||(!excludeCalled.empty()&&matches(targetCall(i),excludeCalled))||(!excludeCountry.empty()&&matches(d.country,excludeCountry))||(!excludeGrid.empty()&&matches(i.grid,excludeGrid)))return false;
 if(!search.empty()&&upper(d.message+" "+d.country+" "+i.transmitter).find(upper(search))==std::string::npos)return false;
 if(!matches(d.source,source)||!matches(i.transmitter,call)||!matches(targetCall(i),called)||!matches(d.country,country)||!matches(d.continent,continent)||!matches(i.grid,grid)||!matches(bandName(d.dial),band)||!matches(d.mode,mode)||!matches(std::to_string(d.dial),frequency)||!matches(i.type,type))return false;
 if(!prefix.empty()){std::stringstream in(prefix);std::string p;bool ok=false;while(std::getline(in,p,','))if(matches(i.transmitter,p+"*"))ok=true;if(!ok)return false;}
 if(!text.empty()&&upper(d.message).find(upper(text))==std::string::npos)return false;
 if(cqOnly&&i.type!="CQ")return false;if(hasGrid&&!d.position)return false;if(hasTarget&&targetCall(i).empty())return false;if(first&&!d.firstCall)return false;
 if(messageKind!=MessageKind::All&&dx::messageKind(i)!=messageKind)return false;
 if(d.snr<minSnr)return false;if(minKm>0||maxKm<40080)if(!d.km||*d.km<minKm||*d.km>maxKm)return false;
 if(maxDt<30&&(!d.dt||std::abs(*d.dt)>maxDt))return false;
 std::array<std::optional<double>,6> values={double(d.snr),d.km,double(d.dial)/1e6,double(d.audio),d.dt,d.bearing};for(size_t n=0;n<values.size();n++)if(!columnRanges[n].accepts(values[n]))return false;return true;
}
bool Filter::hasColumnRanges()const{return std::any_of(columnRanges.begin(),columnRanges.end(),[](const NumericRange& range){return range.active();});}
bool Filter::accepts(const Row& row,int64_t referenceUtc)const{if(!accepts(row.decode))return false;auto age=[&](int64_t utc)->std::optional<double>{return utc>0?std::optional<double>{double(std::max<int64_t>(0,referenceUtc-utc))}:std::nullopt;};std::array<std::optional<double>,4> values={double(row.count),age(row.decode.utc),age(row.firstHeardUtc),age(row.stationFirstHeardUtc)};for(size_t i=0;i<values.size();i++)if(!columnRanges[i+6].accepts(values[i]))return false;return true;}
Json Filter::json()const{return {{"source",source},{"search",search},{"call",call},{"called",called},{"prefix",prefix},{"country",countryName(country)},{"continent",continent},{"grid",grid},{"band",band},{"mode",mode},{"frequency",frequency},{"text",text},{"type",type},{"excludeCall",excludeCall},{"excludeCalled",excludeCalled},{"excludeCountry",excludeCountry},{"excludeGrid",excludeGrid},{"messageKind",int(messageKind)},{"removeLikelyFalse",removeLikelyFalse},{"includeQuestionable",includeQuestionable},{"falseToleranceKm",falseToleranceKm},{"cqOnly",cqOnly},{"hasGrid",hasGrid},{"hasTarget",hasTarget},{"first",first},{"columnRanges",columnRangesJson(columnRanges)},{"minKm",minKm},{"maxKm",maxKm},{"minSnr",minSnr},{"maxDt",maxDt}};}
Filter Filter::fromJson(const Json& j){Filter f;
#define STR(k) f.k=j.value(#k,"")
 STR(source);STR(search);STR(call);STR(called);STR(prefix);STR(country);STR(continent);STR(grid);STR(band);STR(mode);STR(frequency);STR(text);STR(type);STR(excludeCall);STR(excludeCalled);STR(excludeCountry);STR(excludeGrid);
#undef STR
 for(auto* pattern:{&f.country,&f.excludeCountry}){const std::string legacy="Fed. Rep. of Germany";size_t pos=0;while((pos=pattern->find(legacy,pos))!=std::string::npos){pattern->replace(pos,legacy.size(),"Germany");pos+=7;}}
 f.includeQuestionable=j.value("includeQuestionable",false);f.messageKind=MessageKind(std::clamp(j.value("messageKind",0),0,4));f.removeLikelyFalse=j.value("removeLikelyFalse",false);f.falseToleranceKm=j.value("falseToleranceKm",300.);if(!std::isfinite(f.falseToleranceKm))f.falseToleranceKm=300.;f.falseToleranceKm=std::clamp(f.falseToleranceKm,0.,2000.);f.cqOnly=j.value("cqOnly",false);f.hasGrid=j.value("hasGrid",false);f.hasTarget=j.value("hasTarget",false);f.columnRanges=columnRangesFromJson(j.value("columnRanges",Json::object()));f.first=j.value("first",false);f.minKm=j.value("minKm",0.);f.maxKm=j.value("maxKm",40080.);f.minSnr=j.value("minSnr",-100.);f.maxDt=j.value("maxDt",30.);return f;
}
std::vector<Preset> presets(){std::vector<Preset> p;for(int64_t f:{1840000,3573000,5357000,7074000,10136000,14074000,18100000,21074000,24915000,28074000,50323000,144174000,222065000,432065000})p.push_back({f,"FT8"});for(int64_t f:{7047500,10140000,14080000,18104000,21140000,28180000})p.push_back({f,"FT4"});return p;}
std::vector<Preset> eligible(double center,double width,const std::vector<Preset>& all){std::vector<Preset> p;for(auto& v:all)if(v.dial>=center-width/2&&v.dial+6000<=center+width/2)p.push_back(v);return p;}
}
