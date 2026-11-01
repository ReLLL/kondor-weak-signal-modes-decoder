// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "js8_messages.h"
#include <commons.h>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <unistd.h>
#ifndef _WIN32
#include <sys/resource.h>
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif
#include <cmath>
#include <cstring>
namespace {
bool transfer(int fd,void* data,size_t size,bool writing){auto* p=static_cast<char*>(data);while(size){auto n=writing?write(fd,p,size):read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=size_t(n);}return true;}
}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--check-runtime")_exit(0);
 if(argc!=1)return 2;
#ifdef _WIN32
 _setmode(0,_O_BINARY);_setmode(1,_O_BINARY);SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#else
 setpriority(PRIO_PROCESS,0,10);
#endif
 int output=dup(1);if(output<0||dup2(2,1)<0)return 2;
 QCoreApplication app(argc,argv);JS8::Decoder decoder;dx::JS8Messages messages;QEventLoop loop;dx::Json results;std::vector<JS8::Event::Decoded> frames;int64_t utc=0,dial=0;bool finished=false;
 QObject::connect(&decoder,&JS8::Decoder::decodeEvent,&app,[&](const JS8::Event::Variant& event){
  if(auto decoded=std::get_if<JS8::Event::Decoded>(&event)){
   auto found=std::find_if(frames.begin(),frames.end(),[&](const auto& f){return f.data==decoded->data&&f.type==decoded->type&&f.mode==decoded->mode&&std::abs(f.frequency-decoded->frequency)<=3;});
   if(found!=frames.end()){if(decoded->snr>found->snr)*found=*decoded;}else if(frames.size()<256)frames.push_back(*decoded);
  }
  if(std::holds_alternative<JS8::Event::DecodeFinished>(event)){finished=true;loop.quit();}
 });
 decoder.start(QThread::LowPriority);
 struct FinishDecoder {JS8::Decoder& decoder;~FinishDecoder(){decoder.quit();}} finish{decoder};
 for(;;){uint32_t length=0;if(!transfer(0,&length,4,false))break;if(!length||length>4096)return 2;std::string header(length,'\0');if(!transfer(0,header.data(),length,false))return 2;
  try{auto request=dx::Json::parse(header);int period=request.at("period_ms");auto count=request.at("samples").get<int64_t>();if(request.at("mode")!="JS8"||(period!=6000&&period!=10000&&period!=15000&&period!=30000)||count!=int64_t(period)*12)return 2;
   std::memset(&dec_data,0,sizeof(dec_data));if(!transfer(0,dec_data.d2,size_t(count)*2,false))return 2;utc=request.at("utc_ms").get<int64_t>()/1000;dial=request.at("frequency");time_t stamp=utc;tm time{};dx::platform::utcTime(&stamp,&time);
   auto& p=dec_data.params;p.nutc=code_time(time.tm_hour,time.tm_min,time.tm_sec);p.nfa=100;p.nfb=5000;p.nfqso=1500;p.newdat=true;p.kin=int(count);
   if(period==15000){p.nsubmodes=1;p.kszA=int(count);}else if(period==10000){p.nsubmodes=2;p.kszB=int(count);}else if(period==6000){p.nsubmodes=4;p.kszC=int(count);}else{p.nsubmodes=8;p.kszE=int(count);}
   results=dx::Json::array();frames.clear();finished=false;decoder.decode();loop.exec();if(!finished)return 3;
   size_t bytes=0,skipped=0;
   for(const auto& frame:frames){bool ambiguous=std::any_of(frames.begin(),frames.end(),[&](const auto& other){return &frame!=&other&&frame.mode==other.mode&&std::abs(frame.frequency-other.frequency)<=10;});for(auto& row:messages.accept(frame,utc,dial,ambiguous)){row["period_ms"]=period;auto size=row.dump().size()+1;if(results.size()<256&&bytes+size<=900000){bytes+=size;results.push_back(std::move(row));}else ++skipped;}}
   auto body=dx::Json{{"id",request.at("id")},{"results",results},{"skipped",skipped}}.dump();length=uint32_t(body.size());if(length>1024*1024||!transfer(output,&length,4,true)||!transfer(output,body.data(),length,true))break;
  }catch(const std::exception&){return 2;}
 }
 close(output);return 0;
}
