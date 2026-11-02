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
  auto request=dx::Json::parse(header,nullptr,false);if(!request.is_object()||!request.contains("id")||!request["id"].is_number_integer()||!request.contains("utc_ms")||!request["utc_ms"].is_number_integer()||!request.contains("samples")||!request["samples"].is_number_integer()||!request.contains("mode")||!request["mode"].is_string()||(request.contains("decode_depth")&&!request["decode_depth"].is_number_integer()))return 2;
  auto countValue=request.at("samples").get<int64_t>(),depth=request.value("decode_depth",int64_t(dx::defaultFtDecodeDepth)),utcMs=request.at("utc_ms").get<int64_t>();std::string mode=request.at("mode");
  if((mode!="FT8"&&mode!="FT4")||countValue!=(mode=="FT8"?180000:90000)||depth<1||depth>3||utcMs<0||utcMs>253402300799999LL)return 2;int count=int(countValue);
  std::vector<short> samples(size_t(count),0);if(!transfer(0,samples.data(),samples.size()*2,false))return 2;
  // Brown creates fresh decoder state for every slot. Preserve that behavior:
  // reusing DecoderMs across alternating modes leaves incompatible cached state.
  auto decoder=std::make_unique<DecoderMs>();
  dx::Json results=dx::Json::array();int malformed=0;
  decoder->setMode(mode=="FT8"?11:13);
  QStringList words;words<<"CALL"<<"CALL";decoder->SetWords(words,0,0);
  QStringList hashes;hashes<<"CALL"<<""<<""<<""<<"";decoder->SetCalsHash(hashes);
  decoder->SetThrLevel(1);decoder->SetDecoderDeep(int(depth));decoder->SetApDecode(false);
  decoder->SetResultsCallback([&](const char* line){
   // Retain MSHV's confidence marker/AP flags and reject malformed numeric fields.
   if(auto result=dx::parseDecoderOutput(mode,line))results.push_back(std::move(*result));else ++malformed;
  });
  time_t epoch=utcMs/1000;tm utc{};dx::platform::utcTime(&epoch,&utc);char stamp[16];strftime(stamp,sizeof(stamp),"%H%M%S",&utc);
  decoder->SetDecode(samples.data(),count,stamp,0,4,false,true,false);
  auto response=dx::Json{{"id",request.at("id")},{"results",results},{"malformed",malformed}}.dump();
  uint32_t size=uint32_t(response.size());if(size>1024*1024||!transfer(output,&size,4,true)||!transfer(output,response.data(),size,true))return 3;
 }
 close(output);return 0;
}
