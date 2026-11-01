// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "store.h"
#include <iostream>
#include <unordered_map>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed "<<__LINE__<<": "<<#x<<'\n';return 1;}}while(0)
namespace {
constexpr int64_t now=1800000000;
dx::Decode reception(int64_t utc,const std::string& message){dx::Decode d;d.utc=utc;d.slotMs=utc*1000;d.dial=7074000;d.mode="FT8";d.audio=1000;d.snr=-15;d.message=message;d.source="fixture";return d;}
std::string token(const std::string& call,int bits){return "<"+std::to_string(bits==10?0:bits==12?1:2)+":"+std::to_string(dx::callHash(call,bits))+">";}
auto country=[](const std::string&){return std::make_pair("United States","NA");};
std::pair<std::string,std::string> collision(int bits){std::unordered_map<uint32_t,std::string> seen;for(int i=0;i<50000;i++){auto call="K"+std::to_string(i)+"ABC";auto hash=dx::callHash(call,bits);auto previous=seen.find(hash);if(previous!=seen.end())return {previous->second,call};seen.emplace(hash,std::move(call));}return {};}
}
int main(){
 const dx::Point home{39.5,33};
 for(int bits:{10,12,22})for(int age:{180,480,719,720,721})for(bool sender:{false,true}){
  dx::Store store(":memory:");auto known=reception(now-age,"CQ K1ABC FN42");CHECK(store.insert(known,home,country));auto hash=token("K1ABC",bits);
  auto message=reception(now,sender?"K2ABC "+hash+" -10":hash+" K2ABC -10");CHECK(store.insert(message,home,country));
  const auto& identity=sender?message.identity.transmitter:message.identity.called;CHECK(identity==(age<=720?"K1ABC":hash));
  CHECK(message.identitySource==(age<=720?"unique-recent-hash":"literal"));if(sender)CHECK(message.country==(age<=720?"United States":""));
  dx::Filter filter;filter.removeLikelyFalse=true;CHECK(store.passesLocationFilter(message,filter)==(age<=720||!sender));
 }
 for(int bits:{10,12,22}){
  auto pair=collision(bits);CHECK(!pair.first.empty()&&pair.first!=pair.second&&dx::callHash(pair.first,bits)==dx::callHash(pair.second,bits));auto hash=token(pair.first,bits);
  for(int olderAge:{600,721}){
   dx::Store store(":memory:");auto first=reception(now-olderAge,"CQ "+pair.first+" FN42"),second=reception(now-480,"CQ "+pair.second+" FN03");CHECK(store.insert(first,home,country)&&store.insert(second,home,country));
   auto ambiguous=reception(now,"K2XYZ "+hash+" -10");CHECK(store.insert(ambiguous,home,country));CHECK(ambiguous.identity.transmitter==(olderAge<=720?hash:pair.second));CHECK(ambiguous.country==(olderAge<=720?"":"United States"));
  }
  dx::Store store(":memory:");auto future=reception(now+1,"CQ K1ABC FN42");CHECK(store.insert(future,home,country));auto unknown=reception(now,"K2ABC "+token("K1ABC",bits)+" -10");CHECK(store.insert(unknown,home,country)&&unknown.identity.transmitter==token("K1ABC",bits)&&unknown.country.empty());
 }
 std::cout<<"12-minute sender/target boundaries, all hash sizes, collisions, expired candidates and future exclusion passed\n";
}
