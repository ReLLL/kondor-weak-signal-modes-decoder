// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <map>
#include <sstream>
#include <regex>
namespace dx {
std::string confidenceSummary(const Decode& d){
 if(!d.diagnostics.quality)return d.diagnostics.suspect?"Low":d.diagnostics.flags.empty()?"--":"Unmarked";
 char text[32];std::snprintf(text,sizeof(text),"%s%.1f",d.diagnostics.suspect?"Low ":"",*d.diagnostics.quality);return text;
}
std::string operatingCall(const std::string& input){
 auto call=upper(input);if(call.size()>2&&call.front()=='<'&&call.back()=='>')call=call.substr(1,call.size()-2);
 auto slash=call.find('/');if(slash==std::string::npos)return call;
 auto first=call.substr(0,slash),last=call.substr(slash+1);
 // /R is the 77-bit rover flag, not a Russian operating prefix.
 if(last=="P"||last=="M"||last=="MM"||last=="AM"||last=="QRP"||last=="R"||last=="A"||(!last.empty()&&last.find_first_not_of("0123456789")==std::string::npos))return first;
 static const std::regex fullCall("[A-Z0-9]{1,3}[0-9]{1,4}[A-Z]{1,5}");
 if(first.size()<=4&&!std::regex_match(first,fullCall))return first;
 if(last.size()<=4&&!std::regex_match(last,fullCall))return last;
 return first;
}
namespace {
std::map<int,std::string> fields(const std::string& mode,const std::string& raw){
 if((mode!="FT8"&&mode!="FT4")||raw.rfind(mode+"_OUT\t",0)!=0||raw.size()>8192||raw.find('\0')!=std::string::npos)return {};
 std::vector<std::string> parts;std::string part;std::istringstream in(raw);while(std::getline(in,part,'\t'))parts.push_back(part);
 std::map<int,std::string> result;
 for(size_t i=3;i+1<parts.size();i+=2){const auto& key=parts[i];if(key.size()!=3||key[0]!='{'||key[2]!='}'||key[1]<'0'||key[1]>'7'||!result.emplace(key[1]-'0',parts[i+1]).second)return {};}
 return result;
}
double number(const std::string& value){size_t used=0;double n=std::stod(value,&used);while(used<value.size()&&std::isspace(static_cast<unsigned char>(value[used])))++used;if(used!=value.size()||!std::isfinite(n))throw std::invalid_argument("Invalid decoder number");return n;}
}
DecoderDiagnostics decoderDiagnostics(const std::string& mode,const std::string& raw){
 DecoderDiagnostics out;if((mode!="FT8"&&mode!="FT4")||raw.rfind(mode+"_OUT\t",0)!=0)return out;
 auto value=[&](const char* key){auto from=raw.find(key);if(from==std::string::npos)return std::string{};from+=5;auto end=raw.find('\t',from);return raw.substr(from,end-from);};
 out.flags=value("\t{5}\t");out.suspect=out.flags.find('?')!=std::string::npos;
 try{auto quality=number(value("\t{6}\t"));if(quality>=0&&quality<=1)out.quality=quality;}catch(const std::exception&){}
 return out;
}
Meaning decodedMeaning(const std::string& mode,const std::string& message,const std::string& raw){
 auto meaning=receptionMeaning(mode,message);
 if((mode=="FT8"||mode=="FT4")&&raw.rfind(mode+"_OUT\t",0)==0){
  // The typed upstream unpacker leaves all three calls empty for text/telemetry.
  // Even text that resembles a CQ/grid must not become location evidence.
  auto from=raw.find("\t{4}\t"),end=raw.find("\t{5}\t",from);if(from!=std::string::npos&&end!=std::string::npos){auto bar=raw.find('|',from+5);if(bar<end&&raw.compare(bar+1,2,";;")==0)return {};}
 }
 return meaning;
}
std::optional<Json> parseDecoderOutput(const std::string& mode,const std::string& raw){
 const auto values=fields(mode,raw);for(int field:{0,1,2,4,5,6,7})if(!values.count(field))return {};
 try{
  const auto snr=number(values.at(1)),audio=number(values.at(7)),dt=number(values.at(2)),quality=number(values.at(6));
  if(snr<-100||snr>100||std::floor(snr)!=snr||audio<0||audio>12000||std::floor(audio)!=audio||quality<0||quality>1)return {};
  auto message=values.at(4);auto bar=message.find('|');if(bar!=std::string::npos)message.resize(bar);
  if(message.empty()||message.size()>200||message.find_first_of("\r\n")!=std::string::npos)return {};
  const auto diagnostics=decoderDiagnostics(mode,raw);
  return Json{{"message",message},{"raw",raw},{"snr",int(snr)},{"dt",dt},{"audio_hz",int(audio)},{"decoder_flags",diagnostics.flags},{"decoder_quality",quality},{"decoder_suspect",diagnostics.suspect}};
 }catch(const std::exception&){return {};}
}
}
