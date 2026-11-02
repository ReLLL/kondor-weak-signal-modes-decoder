// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include "alerts.h"
#include "sharp_host.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <objidl.h>
#include <gdiplus.h>
#include <sstream>
#include <map>
#include <cmath>
namespace dx {
namespace {
std::vector<unsigned char> icon(const std::string& name){
 struct Imaging {ULONG_PTR token=0;Imaging(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}~Imaging(){if(token)Gdiplus::GdiplusShutdown(token);}};
 static Imaging imaging;if(!imaging.token)return {};
 auto path=sharpHost().binary/"icons"/std::filesystem::u8path(name);Gdiplus::Bitmap original(path.c_str());if(original.GetLastStatus()!=Gdiplus::Ok)return {};
 Gdiplus::Bitmap scaled(64,64,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&scaled);graphics.Clear(Gdiplus::Color(0,0,0,0));graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);graphics.DrawImage(&original,0,0,64,64);
 Gdiplus::Rect rectangle(0,0,64,64);Gdiplus::BitmapData data{};if(scaled.LockBits(&rectangle,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok)return {};
 std::vector<unsigned char> result(64*64*4);for(int y=0;y<64;y++)for(int x=0;x<64;x++){auto* pixel=static_cast<unsigned char*>(data.Scan0)+y*data.Stride+x*4;auto at=size_t(y*64+x)*4;result[at]=pixel[2];result[at+1]=pixel[1];result[at+2]=pixel[0];result[at+3]=pixel[3];}scaled.UnlockBits(&data);return result;
}
void open(const std::string& url){if(url.rfind("https://",0)!=0)return;auto wide=std::filesystem::u8path(url).wstring();ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
}
std::vector<unsigned char> homeIconPixels(){return icon("1f3e0.png");}
std::vector<unsigned char> trophyIconPixels(){return icon("1f3c6.png");}
std::vector<unsigned char> flagIconPixels(const std::string& code){if(code.empty())return icon("1f3f3.png");if(code.size()!=2||code[0]<'A'||code[0]>'Z'||code[1]<'A'||code[1]>'Z')return {};std::ostringstream name;name<<std::hex<<(0x1f1e6+code[0]-'A')<<'-'<<(0x1f1e6+code[1]-'A')<<".png";return icon(name.str());}
void openCallsignLookup(const std::string& call,int provider){open(callsignLookupUrl(call,provider));}
void openFeedbackProfile(){open("https://x.com/FoxOnTheRunTr");}
void openMapCredit(int index){const char* links[]={"https://www.openstreetmap.org/copyright","https://www.hotosm.org","https://www.openstreetmap.fr","https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer"};if(index>=0&&index<4)open(links[index]);}
std::optional<std::string> chooseCsvLog(const std::string& current){
 wchar_t buffer[32768]{};auto path=std::filesystem::u8path(current).wstring();if(path.size()<32768)std::copy(path.begin(),path.end(),buffer);
 OPENFILENAMEW picker{};picker.lStructSize=sizeof(picker);picker.lpstrFile=buffer;picker.nMaxFile=32768;picker.lpstrTitle=L"Reception CSV log";picker.lpstrFilter=L"Reception CSV (*.csv)\0*.csv\0\0";picker.lpstrDefExt=L"csv";picker.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetSaveFileNameW(&picker))return {};return std::filesystem::path(buffer).u8string();
}
bool playAlertSound(const std::string& name){
 if(name=="None")return true;int index=-1;for(int n=0;n<15;n++)if(name==alertSoundNames[n])index=n;if(index<0)return false;
 // Original short chimes, rather than redistributing Apple's system recordings.
 static std::map<int,std::vector<char>> waves;auto& wave=waves[index];
 if(wave.empty()){int samples=12000;auto word=[&](uint32_t value,int count){for(int n=0;n<count;n++)wave.push_back(char(value>>(n*8)));};
  auto text=[&](const char* value,int count){wave.insert(wave.end(),value,value+count);};text("RIFF",4);word(36+samples*2,4);text("WAVEfmt ",8);word(16,4);word(1,2);word(1,2);word(24000,4);word(48000,4);word(2,2);word(16,2);text("data",4);word(samples*2,4);
  for(int n=0;n<samples;n++){double t=n/24000.,envelope=std::pow(std::sin(3.141592653589793*n/samples),2);double frequency=330+index*37;auto value=int16_t(6000*envelope*(std::sin(6.28318530718*frequency*t)+.3*std::sin(6.28318530718*frequency*1.5*t)));word(uint16_t(value),2);}
 }
 return PlaySoundA(wave.data(),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT)!=FALSE;
}
}
