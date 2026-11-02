// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "share_card.h"
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstring>
#include <fstream>
#include <mutex>
namespace dx {
namespace {
struct Imaging {ULONG_PTR token=0;Imaging(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}~Imaging(){if(token)Gdiplus::GdiplusShutdown(token);}};
Imaging& imaging(){static Imaging value;return value;}
std::wstring wide(const std::string& value){return std::filesystem::u8path(value).wstring();}
Gdiplus::Color color(ShareColor c){return {c.a,c.r,c.g,c.b};}
bool clip(UINT format,const void* bytes,size_t length){auto block=GlobalAlloc(GMEM_MOVEABLE,length);if(!block)return false;auto* target=GlobalLock(block);if(!target){GlobalFree(block);return false;}std::memcpy(target,bytes,length);GlobalUnlock(block);if(!SetClipboardData(format,block)){GlobalFree(block);return false;}return true;}
}
ShareImage renderShareCard(const ShareCard& card){
 ShareImage result;if(!imaging().token){result.error="Could not start image rendering.";return result;}
 Gdiplus::Bitmap bitmap(shareWidth,shareHeight,PixelFormat32bppARGB);
 {Gdiplus::Graphics g(&bitmap);g.Clear(Gdiplus::Color(255,0,0,0));g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
 for(const auto& m:shareComposition(card)){
  Gdiplus::SolidBrush brush(color(m.color));Gdiplus::Pen pen(color(m.color),m.size);
  switch(m.kind){
   case ShareMark::Kind::Background:{
    auto bytes=shareBackground();auto memory=GlobalAlloc(GMEM_MOVEABLE,bytes.size);if(!memory)break;auto* target=GlobalLock(memory);if(!target){GlobalFree(memory);break;}std::memcpy(target,bytes.data,bytes.size);GlobalUnlock(memory);IStream* stream=nullptr;
    if(CreateStreamOnHGlobal(memory,TRUE,&stream)==S_OK){{Gdiplus::Bitmap photo(stream);if(photo.GetLastStatus()==Gdiplus::Ok)g.DrawImage(&photo,m.x,m.y,m.w,m.h);}stream->Release();}else GlobalFree(memory);break;
   }
   case ShareMark::Kind::Rectangle:g.FillRectangle(&brush,m.x,m.y,m.w,m.h);break;
   case ShareMark::Kind::Line:g.DrawLine(&pen,m.x,m.y,m.w,m.h);break;
   case ShareMark::Kind::Ellipse:g.DrawEllipse(&pen,m.x,m.y,m.w,m.h);break;
   case ShareMark::Kind::Trophy:{Gdiplus::Font face(L"Segoe UI Emoji",m.h,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);g.DrawString(L"\U0001F3C6",-1,&face,Gdiplus::PointF(m.x,m.y),&brush);break;}
   case ShareMark::Kind::Text:{
    auto value=wide(m.text+m.accent);float size=m.size;Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap|Gdiplus::StringFormatFlagsMeasureTrailingSpaces);format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    Gdiplus::RectF box(m.x,m.y,m.w,m.h),bounds;
    for(;;){Gdiplus::Font face(L"Segoe UI",size,m.bold?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);g.MeasureString(value.c_str(),-1,&face,Gdiplus::PointF(0,0),&format,&bounds);if(bounds.Width<=m.w||size<=m.size*.65f){
     if(m.accent.empty())g.DrawString(value.c_str(),-1,&face,box,&format,&brush);
     else {auto prefix=wide(m.text),suffix=wide(m.accent);Gdiplus::RectF prefixBounds;g.MeasureString(prefix.c_str(),-1,&face,Gdiplus::PointF(0,0),&format,&prefixBounds);g.DrawString(prefix.c_str(),-1,&face,box,&format,&brush);Gdiplus::SolidBrush accent(color(m.accentColor));Gdiplus::RectF end(box.X+prefixBounds.Width,box.Y,std::max(0.f,box.Width-prefixBounds.Width),box.Height);g.DrawString(suffix.c_str(),-1,&face,end,&format,&accent);}break;
    }size-=1;}
    break;
   }
  }
 }}
 Gdiplus::BitmapData data{};Gdiplus::Rect rect(0,0,shareWidth,shareHeight);
 if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok){result.error="Could not read image pixels.";return result;}
 result.rgba.resize(size_t(shareWidth)*shareHeight*4);
 for(int y=0;y<shareHeight;y++)for(int x=0;x<shareWidth;x++){auto* p=static_cast<unsigned char*>(data.Scan0)+y*data.Stride+x*4;auto* q=result.rgba.data()+size_t(y*shareWidth+x)*4;q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=p[3];}bitmap.UnlockBits(&data);
 IStream* stream=nullptr;if(CreateStreamOnHGlobal(nullptr,TRUE,&stream)!=S_OK){result.error="Could not encode the share image.";return result;}
 // Microsoft PNG encoder CLSID, independent of localized codec names.
 const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
 if(bitmap.Save(stream,&png,nullptr)==Gdiplus::Ok){HGLOBAL buffer=nullptr;GetHGlobalFromStream(stream,&buffer);STATSTG stat{};if(stream->Stat(&stat,STATFLAG_NONAME)==S_OK&&stat.cbSize.QuadPart<16*1024*1024){auto* bytes=static_cast<const unsigned char*>(GlobalLock(buffer));if(bytes){result.png.assign(bytes,bytes+size_t(stat.cbSize.QuadPart));GlobalUnlock(buffer);}}}stream->Release();if(!result.valid())result.error="Could not encode the share image.";return result;
}
bool copyShareImage(const ShareImage& image,const std::string& caption){
 auto owner=GetActiveWindow();if(!image.valid()||!owner||!OpenClipboard(owner))return false;EmptyClipboard();
 BITMAPV5HEADER header{};header.bV5Size=sizeof(header);header.bV5Width=shareWidth;header.bV5Height=-shareHeight;header.bV5Planes=1;header.bV5BitCount=32;header.bV5Compression=BI_BITFIELDS;header.bV5RedMask=0xff0000;header.bV5GreenMask=0xff00;header.bV5BlueMask=0xff;header.bV5AlphaMask=0xff000000;header.bV5CSType=LCS_sRGB;
 std::vector<unsigned char> dib(sizeof(header)+image.rgba.size());std::memcpy(dib.data(),&header,sizeof(header));
 for(size_t i=0;i<image.rgba.size();i+=4){auto* q=dib.data()+sizeof(header)+i;q[0]=image.rgba[i+2];q[1]=image.rgba[i+1];q[2]=image.rgba[i];q[3]=image.rgba[i+3];}
 bool success=clip(CF_DIBV5,dib.data(),dib.size());auto format=RegisterClipboardFormatW(L"PNG");if(format)success=clip(format,image.png.data(),image.png.size())||success;
 auto text=wide(caption);clip(CF_UNICODETEXT,text.c_str(),(text.size()+1)*sizeof(wchar_t));CloseClipboard();return success;
}
std::string saveShareImage(const ShareImage& image,int64_t utc){
 if(!image.valid())return "Error: No image is ready.";wchar_t path[32768]{};auto name=wide("Kondor-reception-"+std::to_string(utc)+".png");std::copy(name.begin(),name.end(),path);
 OPENFILENAMEW picker{};picker.lStructSize=sizeof(picker);picker.lpstrFile=path;picker.nMaxFile=32768;picker.lpstrTitle=L"Save reception highlights";picker.lpstrFilter=L"PNG image (*.png)\0*.png\0\0";picker.lpstrDefExt=L"png";picker.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetSaveFileNameW(&picker))return {};std::ofstream out(std::filesystem::path(path),std::ios::binary);out.write(reinterpret_cast<const char*>(image.png.data()),std::streamsize(image.png.size()));out.close();return out?std::filesystem::path(path).u8string():"Error: Could not save the PNG image.";
}
void openShareService(ShareService service,const ShareCard& card){auto url=wide(shareServiceUrl(service,card));ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
}
