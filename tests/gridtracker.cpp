// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "gridtracker.h"
#include "temporary_folder.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
struct Reader {
 const dx::wsjt::Packet& p;size_t offset=0;
 uint64_t number(int size=4){uint64_t v=0;for(int i=0;i<size;i++)v=v<<8|p.at(offset++);return v;}
 std::string text(){auto size=number();std::string s(p.begin()+offset,p.begin()+offset+size);offset+=size;return s;}
 double real(){auto bits=number(8);double d;std::memcpy(&d,&bits,8);return d;}
};
int main(int argc,char** argv){
 int receiver=socket(AF_INET,SOCK_DGRAM,0);CHECK(receiver>=0);sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);CHECK(bind(receiver,reinterpret_cast<sockaddr*>(&local),sizeof(local))==0);socklen_t length=sizeof(local);CHECK(getsockname(receiver,reinterpret_cast<sockaddr*>(&local),&length)==0);CHECK(fcntl(receiver,F_SETFL,O_NONBLOCK)==0);
 sockaddr_in sender{};auto read=[&]{std::vector<dx::wsjt::Packet> packets;uint8_t bytes[4096];pollfd ready{receiver,POLLIN,0};while(poll(&ready,1,10)>0){socklen_t size=sizeof(sender);auto n=recvfrom(receiver,bytes,sizeof(bytes),0,reinterpret_cast<sockaddr*>(&sender),&size);if(n<0)break;packets.emplace_back(bytes,bytes+n);}return packets;};
 auto pattern=(std::filesystem::temp_directory_path()/"dx-gridtracker-XXXXXX").string();CHECK(mkdtemp(pattern.data()));std::filesystem::path folder=pattern;TestFolderCleanup cleanup(folder);dx::Store store(folder/"history.sqlite3");int64_t now=1800000100;dx::Decode d;d.utc=now-60;d.dial=14074000;d.mode="FT8";d.snr=-17;d.dt=.35;d.audio=1245;d.message="CQ K1ABC FN42";
 auto insert=[&](dx::Decode row){return store.insert(row,{39,33},[](const std::string&){return std::make_pair("United States","NA");});};CHECK(insert(d));auto ft4=d;ft4.mode="FT4";ft4.dial=14080000;CHECK(insert(ft4));auto legacy=d;legacy.source="legacy";legacy.audio++;CHECK(insert(legacy));auto recording=d;recording.source="replay";recording.audio+=2;CHECK(insert(recording));auto yesterday=d;yesterday.utc=now/86400*86400-1;CHECK(insert(yesterday));
 dx::GridTracker client;dx::GridTrackerContext context;context.channels={{d.dial,d.mode},{ft4.dial,ft4.mode}};context.grid="KM69";dx::Json options={{"address","127.0.0.1"},{"port",ntohs(local.sin_port)}};
 client.tick(store,options,context,true,now,10000);CHECK(read().empty()&&!client.state().enabled);options["enabled"]=true;client.tick(store,options,context,true,now,10100);auto packets=read();int decodes=0;std::set<std::string> ids;
 for(const auto& p:packets){Reader r{p};CHECK(r.number()==0xadbccbda&&r.number()==2);auto kind=r.number();auto id=r.text();ids.insert(id);if(kind==2){CHECK(r.number(1)==0&&r.number()==uint64_t(d.utc%86400*1000));CHECK(int32_t(r.number())==-17&&std::abs(r.real()-.35)<1e-12&&r.number()==1245);CHECK(r.text()==(id.find("FT4")!=std::string::npos?"+":"~"));CHECK(r.text()==d.message&&r.number(1)==0&&r.number(1)==0&&r.offset==p.size());++decodes;}
  if(kind==1){CHECK(r.number(8)==uint64_t(id==dx::wsjt::identity({d.dial,d.mode})?d.dial:ft4.dial));CHECK(r.text()==(id.find("FT4")!=std::string::npos?"FT4":"FT8"));CHECK(r.text().empty()&&r.text().empty());r.text();CHECK(r.number(1)==0&&r.number(1)==0&&r.number(1)==1);r.number();r.number();CHECK(r.text()=="SWL"&&r.text()=="KM69");}
 }
 CHECK(decodes==2&&ids.size()==2&&client.state().sent==2);
 auto suspect=d;suspect.utc=now;suspect.message="CQ K9BAD AA00";suspect.diagnostics.suspect=true;CHECK(insert(suspect));client.tick(store,options,context,true,now,10150);CHECK(read().empty()&&client.state().sent==2);
 if(argc==2){std::ofstream file(argv[1],std::ios::binary);auto packet=dx::wsjt::decode(d,true);file.write(reinterpret_cast<const char*>(packet.data()),packet.size());}
 auto fresh=d;fresh.utc=now;fresh.dial=7074000;CHECK(insert(fresh));client.tick(store,options,context,true,now,10200);packets=read();decodes=0;for(const auto& p:packets){Reader r{p};r.number();r.number();if(r.number()!=2)continue;CHECK(r.text()==dx::wsjt::identity({fresh.dial,fresh.mode})&&r.number(1)==1);++decodes;}CHECK(decodes==1&&client.state().sent==3);
 auto replay=dx::wsjt::header(7,dx::wsjt::identity({d.dial,d.mode}));CHECK(sendto(receiver,replay.data(),replay.size(),0,reinterpret_cast<sockaddr*>(&sender),sizeof(sender))==ssize_t(replay.size()));decodes=0;for(int attempt=0;attempt<50&&decodes<3;attempt++){client.tick(store,options,context,true,now,13200);packets=read();for(const auto& p:packets){Reader r{p};r.number();r.number();if(r.number()!=2)continue;r.text();CHECK(r.number(1)==0);++decodes;}}CHECK(decodes==3&&client.state().lastReply==now);
 auto control=dx::wsjt::header(15,dx::wsjt::identity({d.dial,d.mode}));sendto(receiver,control.data(),control.size(),0,reinterpret_cast<sockaddr*>(&sender),sizeof(sender));client.tick(store,options,context,true,now,13300);CHECK(read().empty());
 auto bad=replay;bad.pop_back();CHECK(!dx::wsjt::request(bad.data(),bad.size()));bad.assign(4096,255);CHECK(!dx::wsjt::request(bad.data(),bad.size()));
 options["enabled"]=false;client.tick(store,options,context,true,now,14000);packets=read();CHECK(packets.size()==3);for(const auto& p:packets){auto request=dx::wsjt::request(p.data(),p.size());CHECK(request&&request->first==6);}CHECK(!client.state().enabled);
 options["enabled"]=true;options["replay"]=false;client.tick(store,options,context,true,now,15000);packets=read();CHECK(packets.size()==4);for(const auto& p:packets){auto request=dx::wsjt::request(p.data(),p.size());CHECK(request&&request->first!=2);}options["address"]="invalid host";client.tick(store,options,context,true,now,16000);CHECK(!client.state().error.empty());
 ::close(receiver);std::cout<<"UDP forwarding, frequency/mode identities, replay, dates, off-air exclusion and ignored controls verified\n";return 0;
}
