// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include <dlfcn.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstdio>
#include <initializer_list>
int main(int argc,char** argv){
 rlimit core{0,0};setrlimit(RLIMIT_CORE,&core);
 if(argc!=3){dprintf(2,"Expected host core and Kondor plugin.\n");_exit(2);}
 if(!dlopen(argv[1],RTLD_NOW|RTLD_GLOBAL)){dprintf(2,"Host runtime: %s\n",dlerror());_exit(3);}
 auto* plugin=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);if(!plugin){dprintf(2,"Plugin runtime: %s\n",dlerror());_exit(4);}
 for(auto name:{"_INFO_","_INIT_","_CREATE_INSTANCE_","_DELETE_INSTANCE_","_END_"})if(!dlsym(plugin,name)){dprintf(2,"Missing entry point: %s\n",name);_exit(5);}
 auto check=reinterpret_cast<bool(*)()>(dlsym(plugin,"_DX_CHECK_HOST_"));if(!check||!check()){dprintf(2,"Host ImGui ABI mismatch. Rebuild against this host's matching source.\n");_exit(6);}
 dprintf(1,"ELF dependency loading, entry points and host ImGui layout passed.\n");
 // Host-global GUI destructors require a GL context. No instance or writer was started.
 _exit(0);
}
