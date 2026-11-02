// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "helper_client.h"
#ifndef _WIN32
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <pthread.h>
#endif
#endif
#include <chrono>
#include <stdexcept>
#ifndef _WIN32
extern char** environ;
#endif
namespace dx {
int64_t monotonicMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
#ifndef _WIN32
void Helper::stop(){if(input_>=0)close(input_);if(output_>=0)close(output_);input_=output_=-1;if(pid_>0){kill(isolated_?-pid_:pid_,SIGKILL);int status;while(waitpid(pid_,&status,0)<0&&errno==EINTR){}}pid_=-1;if(!work_.empty()){std::error_code error;std::filesystem::remove_all(work_,error);work_.clear();}}
void Helper::start(){
 if(isolated_){auto pattern=(std::filesystem::temp_directory_path()/"dx-mode-worker-XXXXXX").string();auto* owned=mkdtemp(pattern.data());if(!owned)throw std::runtime_error("Decoder scratch unavailable");work_=owned;}
 int in[2],out[2];if(pipe(in))throw std::runtime_error("Helper pipe unavailable");if(pipe(out)){close(in[0]);close(in[1]);throw std::runtime_error("Helper pipe unavailable");}
 for(int fd:{in[0],in[1],out[0],out[1]})fcntl(fd,F_SETFD,FD_CLOEXEC);
 posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
 posix_spawn_file_actions_adddup2(&actions,in[0],0);posix_spawn_file_actions_adddup2(&actions,out[1],1);
 posix_spawn_file_actions_addopen(&actions,2,"/dev/null",O_WRONLY,0);
 posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);if(isolated_){posix_spawn_file_actions_addchdir_np(&actions,work_.c_str());posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attributes,0);}
 auto path=binary_.string();char* argv[]={path.data(),nullptr};int error=posix_spawn(&pid_,path.c_str(),&actions,&attributes,argv,environ);posix_spawnattr_destroy(&attributes);
 posix_spawn_file_actions_destroy(&actions);close(in[0]);close(out[1]);
 if(error){close(in[1]);close(out[0]);pid_=-1;throw std::runtime_error("Decoder helper could not start");}
 input_=in[1];output_=out[0];fcntl(input_,F_SETFL,O_NONBLOCK);fcntl(output_,F_SETFL,O_NONBLOCK);
#ifdef F_SETNOSIGPIPE
 fcntl(input_,F_SETNOSIGPIPE,1);
#endif
}
void Helper::transfer(int fd,void* data,size_t size,bool writing,int64_t deadline,const std::atomic<bool>& stopping){
 auto* p=static_cast<char*>(data);while(size){if(stopping||monotonicMs()>deadline)throw std::runtime_error(stopping?"Stopping":isolated_?"Additional-mode decoder timed out":"Decoder exceeded 20 seconds");
  pollfd ready{fd,short(writing?POLLOUT:POLLIN),0};int r=poll(&ready,1,100);if(r<0&&errno==EINTR)continue;if(r<0||ready.revents&(POLLERR|POLLNVAL))throw std::runtime_error("Decoder pipe failed");if(!r)continue;
  ssize_t n;
#if defined(__linux__)
  // A dead helper must not deliver SIGPIPE to its SDR host. Block only this
  // worker's write, consume only the signal it generated, and restore its mask.
  if(writing){sigset_t mask,old,pending;sigemptyset(&mask);sigaddset(&mask,SIGPIPE);pthread_sigmask(SIG_BLOCK,&mask,&old);sigpending(&pending);bool existed=sigismember(&pending,SIGPIPE);n=write(fd,p,size);int error=errno;if(n<0&&error==EPIPE&&!existed){timespec zero{};while(sigtimedwait(&mask,nullptr,&zero)<0&&errno==EINTR){}}pthread_sigmask(SIG_SETMASK,&old,nullptr);errno=error;}else n=read(fd,p,size);
#else
  n=writing?write(fd,p,size):read(fd,p,size);
#endif
  if(n<0&&(errno==EAGAIN||errno==EINTR))continue;if(n<=0)throw std::runtime_error("Decoder helper exited");p+=n;size-=size_t(n);
 }
}
#endif
Json Helper::decode(const Job& job,const std::atomic<bool>& stopping){
 try{size_t count=job.pcm.size();std::vector<std::pair<const int16_t*,size_t>> spans;
  if(!job.audio.empty()){
   if(!job.pcm.empty()||job.periodMs<=0||job.periodMs>1800000||job.utcMs<0||job.utcMs>INT64_MAX/12-int64_t(job.periodMs))throw std::runtime_error("Invalid captured audio window");
   int64_t cursor=job.utcMs*12,end=cursor+int64_t(job.periodMs)*12;count=size_t(end-cursor);
   for(const auto& part:job.audio){if(!part)throw std::runtime_error("Missing captured audio block");auto from=std::max(cursor,part->startSample),to=std::min(end,part->startSample+int64_t(part->pcm.size()));if(to<=from)continue;if(from!=cursor)throw std::runtime_error("Incomplete captured audio window");spans.push_back({part->pcm.data()+size_t(from-part->startSample),size_t(to-from)});cursor=to;}
   if(cursor!=end)throw std::runtime_error("Incomplete captured audio window");
  }
  if(pid_<0)start();auto header=Json{{"id",job.id},{"utc_ms",job.utcMs},{"frequency",job.dial},{"mode",job.mode},{"decode_depth",job.decodeDepth},{"period_ms",job.periodMs},{"submode",job.submode},{"samples",count}}.dump();uint32_t size=uint32_t(header.size());auto deadline=monotonicMs()+(timeoutMs_>0?timeoutMs_:isolated_?120000:20000);
  transfer(input_,&size,4,true,deadline,stopping);transfer(input_,header.data(),size,true,deadline,stopping);
  if(spans.empty())transfer(input_,const_cast<int16_t*>(job.pcm.data()),job.pcm.size()*2,true,deadline,stopping);
  else for(const auto& span:spans)transfer(input_,const_cast<int16_t*>(span.first),span.second*2,true,deadline,stopping);
  transfer(output_,&size,4,false,deadline,stopping);if(size>1024*1024)throw std::runtime_error("Invalid decoder response size");
  std::string body(size,'\0');transfer(output_,body.data(),size,false,deadline,stopping);auto result=Json::parse(body);
  if(result.at("id")!=job.id||!result.at("results").is_array())throw std::runtime_error("Stale decoder response");if(!result.value("error",std::string{}).empty())throw std::runtime_error(result.at("error").get<std::string>());return result;
 }catch(...){stop();throw;}
}
}
