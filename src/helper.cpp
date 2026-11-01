// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include "ft8_etc/decoderms.h"
#include <unistd.h>
#include <sstream>
#include <memory>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
namespace {
bool transfer(int fd,void* data,size_t size,bool writing){auto* p=static_cast<char*>(data);while(size){ssize_t n=writing?write(fd,p,size):read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=size_t(n);}return true;}
}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--check-runtime")_exit(0);
#ifdef _WIN32
 _setmode(0,_O_BINARY);_setmode(1,_O_BINARY);
#endif
 // Preserve a dedicated IPC descriptor: upstream diagnostic output cannot corrupt frames.
 int output=dup(STDOUT_FILENO);dup2(STDERR_FILENO,STDOUT_FILENO);
 mshv_init();
 for(;;){uint32_t length=0;if(!transfer(0,&length,4,false))break;if(length==0||length>4096)return 2;
  std::string header(length,'\0');if(!transfer(0,header.data(),length,false))return 2;
  auto request=dx::Json::parse(header,nullptr,false);if(request.is_discarded())return 2;
  int count=request.value("samples",0);std::string mode=request.value("mode","");
  if((mode!="FT8"&&mode!="FT4")||count!= (mode=="FT8"?180000:90000))return 2;
  std::vector<short> samples(size_t(count),0);if(!transfer(0,samples.data(),samples.size()*2,false))return 2;
  // Brown creates fresh decoder state for every slot. Preserve that behavior:
  // reusing DecoderMs across alternating modes leaves incompatible cached state.
  auto decoder=std::make_unique<DecoderMs>();
  dx::Json results=dx::Json::array();int malformed=0;
  decoder->setMode(mode=="FT8"?11:13);
  QStringList words;words<<"CALL"<<"CALL";decoder->SetWords(words,0,0);
  QStringList hashes;hashes<<"CALL"<<""<<""<<""<<"";decoder->SetCalsHash(hashes);
  decoder->SetThrLevel(1);decoder->SetDecoderDeep(3);
  decoder->SetResultsCallback([&](const char* line){
   // MSHV emits indexed {n} fields. Parse by field labels, never unchecked array offsets.
   std::vector<std::string> fields;std::string f;std::stringstream stream(line);while(std::getline(stream,f,'\t'))fields.push_back(f);
   std::map<int,std::string> values;
   for(size_t i=3;i+1<fields.size();i+=2){if(fields[i].size()<3||fields[i].front()!='{'||fields[i].back()!='}')continue;try{values.emplace(std::stoi(fields[i].substr(1)),fields[i+1]);}catch(const std::exception&){}}
   if(!values.count(1)||!values.count(2)||!values.count(4)||!values.count(7)){++malformed;return;}
   try{auto msg=values.at(4);auto bar=msg.find('|');if(bar!=std::string::npos)msg.resize(bar);
    if(msg.size()>200){++malformed;return;}int snr=std::stoi(values.at(1)),audio=std::stoi(values.at(7));double dt=std::stod(values.at(2));
    if(snr<-100||snr>100||audio<0||audio>12000||!std::isfinite(dt)){++malformed;return;}
    results.push_back({{"message",msg},{"raw",std::string(line)},{"snr",snr},{"dt",dt},{"audio_hz",audio}});
   }catch(const std::exception&){++malformed;}
  });
  time_t epoch=request.at("utc_ms").get<int64_t>()/1000;tm utc{};dx::platform::utcTime(&epoch,&utc);char stamp[16];strftime(stamp,sizeof(stamp),"%H%M%S",&utc);
  decoder->SetDecode(samples.data(),count,stamp,0,4,false,true,false);
  auto response=dx::Json{{"id",request.at("id")},{"results",results},{"malformed",malformed}}.dump();
  uint32_t size=uint32_t(response.size());if(size>1024*1024||!transfer(output,&size,4,true)||!transfer(output,response.data(),size,true))return 3;
 }
 close(output);return 0;
}
