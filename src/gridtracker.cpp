// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "gridtracker.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <cstring>
#include <cerrno>
#include <cmath>
namespace dx {
namespace {
std::string socketError(){
#ifdef _WIN32
 return "UDP error "+std::to_string(WSAGetLastError());
#else
 return std::strerror(errno);
#endif
}
void closeSocket(intptr_t socket){
#ifdef _WIN32
 closesocket(SOCKET(socket));
#else
 ::close(int(socket));
#endif
}
bool socketReady(){
#ifdef _WIN32
 struct Runtime {bool ok;Runtime(){WSADATA data{};ok=WSAStartup(MAKEWORD(2,2),&data)==0;}~Runtime(){if(ok)WSACleanup();}};
 static Runtime runtime;return runtime.ok;
#else
 return true;
#endif
}
bool nonblocking(intptr_t socket){
#ifdef _WIN32
 u_long enabled=1;return ioctlsocket(SOCKET(socket),FIONBIO,&enabled)==0;
#else
 return fcntl(int(socket),F_SETFL,O_NONBLOCK)>=0;
#endif
}
}
namespace wsjt {
namespace {
void integer(Packet& p,uint64_t value,int width=4){for(int i=width-1;i>=0;i--)p.push_back(uint8_t(value>>(i*8)));}
void text(Packet& p,const std::string& value){integer(p,value.size());p.insert(p.end(),value.begin(),value.end());}
uint32_t read(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
}
std::string identity(const Preset& p){return "Kondor "+p.mode+" "+std::to_string(p.dial)+" Hz";}
Packet header(uint32_t type,const std::string& id){Packet p;integer(p,0xadbccbda);integer(p,2);integer(p,type);text(p,id);return p;}
Packet heartbeat(const std::string& id){auto p=header(0,id);integer(p,2);text(p,DX_VERSION);text(p,"receive-only");return p;}
Packet status(const Preset& preset,const std::string& call,const std::string& grid,bool receiving){
 auto p=header(1,identity(preset));integer(p,uint64_t(preset.dial),8);text(p,preset.mode);text(p,"");text(p,"");text(p,preset.mode);
 p.insert(p.end(),{0,0,uint8_t(receiving)});integer(p,0);integer(p,0);text(p,call.empty()?"SWL":call);text(p,grid);text(p,"");p.push_back(0);text(p,"");p.insert(p.end(),{0,0});integer(p,50);integer(p,preset.mode=="FT4"?0xffffffff:15);text(p,"Kondor receive only");text(p,"");return p;
}
Packet decode(const Decode& d,bool fresh){auto p=header(2,identity({d.dial,d.mode}));p.push_back(uint8_t(fresh));integer(p,uint32_t((d.utc%86400+86400)%86400*1000));integer(p,uint32_t(d.snr));
 double dt=d.dt.value_or(0);uint64_t bits=0;static_assert(sizeof(dt)==sizeof(bits));std::memcpy(&bits,&dt,sizeof(bits));integer(p,bits,8);integer(p,uint32_t(std::max(0,d.audio)));text(p,d.mode=="FT4"?"+":"~");text(p,d.message);p.insert(p.end(),{0,uint8_t(d.source=="replay")});return p;
}
std::optional<std::pair<uint32_t,std::string>> request(const uint8_t* p,size_t size){if(size<16||read(p)!=0xadbccbda||read(p+4)<2||read(p+4)>3)return {};auto length=read(p+12);if(length>256||length>size-16)return {};return std::make_pair(read(p+8),std::string(reinterpret_cast<const char*>(p+16),length));}
}
bool GridTracker::send(const wsjt::Packet& packet){if(socket_<0)return false;if(::send(socket_,reinterpret_cast<const char*>(packet.data()),int(packet.size()),0)!=int(packet.size())){state_.error=socketError();return false;}state_.error.clear();return true;}
void GridTracker::close(){if(socket_>=0){for(const auto& item:sessions_)send(wsjt::header(6,item.first));closeSocket(socket_);socket_=-1;}sessions_.clear();announced_.clear();pending_.clear();lastPulse_=0;state_.lastReply=0;}
void GridTracker::remember(const Preset& p){if(p.dial<=0||(p.mode!="FT8"&&p.mode!="FT4"))return;if(sessions_.size()<64||sessions_.count(wsjt::identity(p)))sessions_[wsjt::identity(p)]=p;}
void GridTracker::replay(Store& store,int64_t utc){
 if(!replay_)return;Query q;q.messages=true;q.descending=true;q.since=std::max(utc-minutes_*60,utc/86400*86400);q.until=utc;
 // QTime carries no date. Keep older days and recording playback off the live map.
 auto rows=store.rows(q,0,5000);for(auto row=rows.rbegin();row!=rows.rend();++row)if(row->decode.source=="live")pending_.push_back({row->decode,false});
}
void GridTracker::tick(Store& store,const Json& settings,const GridTrackerContext& context,bool receiving,int64_t utc,int64_t milliseconds){
 bool enabled=settings.value("enabled",false);auto address=settings.value("address",std::string("127.0.0.1"));int port=std::clamp(settings.value("port",2237),1,65535);minutes_=std::clamp(settings.value("replay_minutes",30),1,120);replay_=settings.value("replay",true);
 if(!enabled){if(state_.enabled)close();state_.enabled=false;state_.error.clear();return;}
 if(!socketReady()){state_.error="UDP networking unavailable";return;}
 bool changed=!state_.enabled||address!=address_||port!=port_;state_.enabled=true;
 if(changed){close();address_=address;port_=port;cursor_=store.lastReceptionId();retryAt_=0;}
 if(socket_<0){if(milliseconds<retryAt_)return;retryAt_=milliseconds+5000;addrinfo hints{};hints.ai_socktype=SOCK_DGRAM;hints.ai_flags=AI_NUMERICHOST|AI_NUMERICSERV;addrinfo* result=nullptr;
  int error=getaddrinfo(address_.c_str(),std::to_string(port_).c_str(),&hints,&result);if(error){state_.error="Enter a valid destination IP address";return;}
  socket_=intptr_t(::socket(result->ai_family,SOCK_DGRAM,0));if(socket_<0||!nonblocking(socket_)||connect(socket_,result->ai_addr,int(result->ai_addrlen))<0){state_.error=socketError();if(socket_>=0)closeSocket(socket_);socket_=-1;freeaddrinfo(result);return;}freeaddrinfo(result);
  if(replay_)cursor_=store.lastReceptionId();replay(store,utc);lastReplay_=milliseconds;lastPulse_=milliseconds-5000;
 }
 for(const auto& p:context.channels)remember(p);
 uint8_t buffer[4096];for(int i=0;i<32;i++){auto count=recv(socket_,reinterpret_cast<char*>(buffer),sizeof(buffer),0);if(count<0)break;auto request=wsjt::request(buffer,size_t(count));if(!request||!sessions_.count(request->second))continue;
  if(request->first==0||request->first==7){state_.lastReply=utc;if(request->first==7&&pending_.empty()&&milliseconds-lastReplay_>=2000){replay(store,utc);lastReplay_=milliseconds;}}
  // Reply, Configure, HaltTx, Clear and all other controls are ignored.
 }
 if(pending_.size()<4000)for(auto& item:store.logRows(cursor_,1000)){cursor_=item.first;const auto& d=item.second;if(d.source=="live"&&d.utc>=std::max(utc-1800,utc/86400*86400)&&d.utc<=utc+30)pending_.push_back({d,true});}
 for(int i=0;i<20&&!pending_.empty();i++){auto item=std::move(pending_.front());pending_.pop_front();const auto& d=item.first;if(d.utc<utc/86400*86400||d.utc>utc+30)continue;Preset preset{d.dial,d.mode};remember(preset);auto id=wsjt::identity(preset);if(!sessions_.count(id))continue;
  if(d.diagnostics.suspect||store.ignoredReception(d))continue;
  if(announced_.insert(id).second)send(wsjt::heartbeat(id));if(send(wsjt::status(preset,context.call,context.grid,receiving))&&send(wsjt::decode(d,item.second)))++state_.sent;
 }
 if(milliseconds-lastPulse_>=5000){for(const auto& item:sessions_){send(wsjt::heartbeat(item.first));send(wsjt::status(item.second,context.call,context.grid,receiving));announced_.insert(item.first);}lastPulse_=milliseconds;}
}
}
