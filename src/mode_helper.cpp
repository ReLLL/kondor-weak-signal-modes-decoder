// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include "modes.h"
#include <unistd.h>
#ifndef _WIN32
#include <sys/resource.h>
#include <sys/wait.h>
#include <spawn.h>
#else
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#endif
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#ifndef _WIN32
extern char** environ;
#else
namespace dx {std::string windowsWsprOutput(const std::filesystem::path&,const std::string&,const std::filesystem::path&);}
#endif
extern "C" void dx_native_decode(int,int,int,int,const int16_t*,int,int,int);
namespace {
dx::Json results;
bool transfer(int fd,void* data,size_t size,bool writing){auto* p=static_cast<char*>(data);while(size){ssize_t n=writing?write(fd,p,size):read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=size_t(n);}return true;}
void add(int snr,double dt,double audio,int used,std::string message){
 while(!message.empty()&&message.back()==' ')message.pop_back();
 if(message.empty()||message.size()>200||snr<-100||snr>100||!std::isfinite(dt)||!std::isfinite(audio)||audio<0||audio>6000||used<1||used>1000||results.size()>=1000)return;
 results.push_back({{"message",message},{"raw",message},{"snr",snr},{"dt",dt},{"audio_hz",int(std::lround(audio))},{"averaged_intervals",used}});
}
void wave(const std::filesystem::path& path,const std::vector<int16_t>& pcm){
 std::ofstream file(path,std::ios::binary|std::ios::trunc);
 auto word=[&](uint32_t value,int count){for(int i=0;i<count;i++)file.put(char((value>>(8*i))&255));};
 file.write("RIFF",4);word(uint32_t(36+2*pcm.size()),4);file.write("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);word(12000,4);word(24000,4);word(2,2);word(16,2);file.write("data",4);word(uint32_t(2*pcm.size()),4);
 file.write(reinterpret_cast<const char*>(pcm.data()),std::streamsize(2*pcm.size()));file.close();if(!file)throw std::runtime_error("Cannot prepare decoder audio");
}
void wspr(const std::filesystem::path& binary,const dx::Json& request,const std::vector<int16_t>& samples){
 time_t epoch=request.at("utc_ms").get<int64_t>()/1000;tm utc{};dx::platform::utcTime(&epoch,&utc);char stamp[40];strftime(stamp,sizeof(stamp),"%y%m%d_%H%M.wav",&utc);std::filesystem::path input=stamp;
 wave(input,samples);
#ifdef _WIN32
 auto output=dx::windowsWsprOutput(binary,std::to_string(request.at("frequency").get<int64_t>()/1e6),input);
 std::filesystem::remove(input);bool failed=false;
#else
 int pipefd[2];if(pipe(pipefd))throw std::runtime_error("WSPR pipe unavailable");
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);posix_spawn_file_actions_adddup2(&actions,pipefd[1],1);posix_spawn_file_actions_addclose(&actions,pipefd[0]);posix_spawn_file_actions_addclose(&actions,pipefd[1]);
 std::string command=binary.string(),dial=std::to_string(request.at("frequency").get<int64_t>()/1e6),filename=input.string();char* args[]={command.data(),const_cast<char*>("-a"),const_cast<char*>("."),const_cast<char*>("-f"),dial.data(),filename.data(),nullptr};
 pid_t child=-1;int error=posix_spawn(&child,command.c_str(),&actions,nullptr,args,environ);posix_spawn_file_actions_destroy(&actions);close(pipefd[1]);
 if(error){close(pipefd[0]);std::filesystem::remove(input);throw std::runtime_error("WSPR decoder could not start");}
 std::string output;char block[4096];bool overflow=false;for(;;){ssize_t n=read(pipefd[0],block,sizeof(block));if(n<0&&errno==EINTR)continue;if(n<=0)break;if(output.size()+size_t(n)<=1024*1024)output.append(block,size_t(n));else overflow=true;}
 close(pipefd[0]);int status=0;while(waitpid(child,&status,0)<0&&errno==EINTR){}std::filesystem::remove(input);
 bool failed=overflow||!WIFEXITED(status)||WEXITSTATUS(status)!=0;
#endif
 // Upstream appends diagnostic receptions; only bounded hash/wisdom state is useful.
 for(const char* name:{"ALL_WSPR.TXT","wspr_spots.txt"})std::filesystem::remove(name);
 if(failed)throw std::runtime_error("WSPR decoder failed");
 std::istringstream lines(output);std::string line;
 while(std::getline(lines,line)){std::istringstream fields(line);int time,snr,drift;double dt,frequency;if(!(fields>>time>>snr>>dt>>frequency>>drift)||time<0||time>2359)continue;std::string message;std::getline(fields,message);auto start=message.find_first_not_of(' ');if(start==std::string::npos)continue;message.erase(0,start);add(snr,dt,(frequency-request.at("frequency").get<int64_t>()/1e6)*1e6,1,message);}
}
}
extern "C" void dx_mode_result(int snr,double dt,double freq,int used,const char* text,int length){if(length>=0&&length<=200)add(snr,dt,freq,used,std::string(text,size_t(length)));}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="--check-runtime")_exit(0);
 if(argc!=1)return 2;auto wsprBinary=dx::platform::executable(std::filesystem::absolute(argv[0]).parent_path()/"dx_wspr_decoder");
#ifdef _WIN32
 _setmode(0,_O_BINARY);_setmode(1,_O_BINARY);SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
#else
 setpriority(PRIO_PROCESS,0,10);
#endif
 int output=dup(1);if(output<0||dup2(2,1)<0)return 2;
 for(;;){uint32_t length=0;if(!transfer(0,&length,4,false))break;if(!length||length>4096)return 2;std::string header(length,'\0');if(!transfer(0,header.data(),length,false))return 2;
  auto request=dx::Json::parse(header,nullptr,false);if(!request.is_object())return 2;
  try{
   auto mode=request.at("mode").get<std::string>();int period=request.value("period_ms",0),submode=request.value("submode",0);auto valid=dx::capturePeriodMs(mode,period);auto count=request.at("samples").get<int64_t>();
   if((mode!="WSPR"&&mode!="FST4"&&mode!="FST4W"&&mode!="Q65")||!valid||count!=*valid*12||submode<0||submode>4||(mode=="Q65"&&submode>(*valid==15000?2:*valid==30000?3:4)))return 2;
   period=int(*valid);std::vector<int16_t> samples(size_t(count),0);if(!transfer(0,samples.data(),samples.size()*2,false))return 2;results=dx::Json::array();std::string error;
   try{if(mode=="WSPR")wspr(wsprBinary,request,samples);else{time_t epoch=request.at("utc_ms").get<int64_t>()/1000;tm utc{};dx::platform::utcTime(&epoch,&utc);int nutc=utc.tm_hour*100+utc.tm_min;if(period<60000)nutc=nutc*100+utc.tm_sec;dx_native_decode(mode=="Q65"?3:mode=="FST4W"?2:1,period/1000,submode,nutc,samples.data(),int(count),0,1);}}
   catch(const std::exception& e){error=e.what();results=dx::Json::array();}
   for(const char* name:{"fst4_decodes.dat","q65_decodes.txt","red.dat","avemsg.txt"})std::filesystem::remove(name);
   for(auto& row:results){row["period_ms"]=period;row["submode"]=submode;}
   auto body=dx::Json{{"id",request.at("id")},{"results",results},{"error",error}}.dump();length=uint32_t(body.size());if(length>1024*1024||!transfer(output,&length,4,true)||!transfer(output,body.data(),length,true))return 3;
  }catch(const std::exception&){return 2;}
 }close(output);return 0;
}
