// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin.h"
#include "sharp_host.h"
#include <core.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <windows.h>
#include <shellapi.h>
#include <iostream>
namespace {
struct OwnedHandle {HANDLE value;~OwnedHandle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct HostGuard {dx::SharpHost& host;~HostGuard(){host.finish();host.consume={};}};
}
int main(){try{
 int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);std::filesystem::path root;DWORD parent=0;
 for(int i=1;i<count;i++){std::wstring arg=args[i];if(arg==L"--data-root"&&i+1<count)root=args[++i];else if(arg==L"--parent"&&i+1<count)parent=DWORD(std::stoul(args[++i]));else throw std::runtime_error("Unsupported worker argument");}LocalFree(args);
 if(root.empty()||!root.is_absolute()||!parent)throw std::runtime_error("Start the decoder from its SDR# plugin");
 OwnedHandle parentHandle{OpenProcess(SYNCHRONIZE,FALSE,parent)};if(!parentHandle.value)throw std::runtime_error("SDR# is no longer running");
 wchar_t binary[32768];auto size=GetModuleFileNameW(nullptr,binary,32768);if(!size||size>=32768)throw std::runtime_error("Worker location unavailable");
 auto& host=dx::sharpHost();host.root=root;host.binary=std::filesystem::path(binary).parent_path();std::filesystem::create_directories(root);
 OwnedHandle ownership{CreateFileW((root/"session.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
 if(ownership.value==INVALID_HANDLE_VALUE)throw std::runtime_error("This reception folder is already open or cannot be written. Close the other decoder instance and retry.");
 core::configManager.load();
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 if(!glfwInit())throw std::runtime_error("Windows graphics initialization failed");glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
 auto* window=glfwCreateWindow(1400,900,DX_PRODUCT_NAME " - SDR#",nullptr,nullptr);if(!window){glfwTerminate();throw std::runtime_error("OpenGL 3 support is required");}
 glfwMakeContextCurrent(window);glfwSwapInterval(1);IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::StyleColorsDark();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 auto settings=(root/"interface.ini").u8string();io.IniFilename=settings.c_str();
 static const ImWchar ranges[]={0x20,0x024f,0x2000,0x27ff,0};ImFontConfig font{};font.SizePixels=15;io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",15,&font,ranges);
 static const ImWchar emoji[]={0x1f300,0x1faff,0};font.MergeMode=true;io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguiemj.ttf",15,&font,emoji);
 ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 130");
 {
  DXNotebook notebook("advanced_dx_sdrsharp");host.consume=[&](dx::SharpAudio audio){notebook.receiveWindowsAudio(std::move(audio));};host.start();HostGuard hostGuard{host};
  while(host.alive&&WaitForSingleObject(parentHandle.value,0)==WAIT_TIMEOUT){
   glfwPollEvents();if(glfwWindowShouldClose(window)){glfwSetWindowShouldClose(window,false);glfwHideWindow(window);notebook.handleDebugCommand("hide","");}
   if(host.show.exchange(false)){glfwShowWindow(window);glfwFocusWindow(window);notebook.showWindows();}
   ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();notebook.renderWindowsFrame();ImGui::Render();
   if(!notebook.windowsVisible())glfwHideWindow(window);
   if(glfwGetWindowAttrib(window,GLFW_VISIBLE)){
    int w,h;glfwGetFramebufferSize(window,&w,&h);glViewport(0,0,w,h);glClearColor(.06f,.08f,.09f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glfwSwapBuffers(window);
   }else glfwWaitEventsTimeout(.05);
  }
 }
 ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
