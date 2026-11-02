// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "native.h"
#include "alerts.h"
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
namespace dx {
namespace {bool nativeInitialized=false;}
bool linuxNativeReady(){static bool ready=[]{
 // GTK registers process-wide GTypes. Keep their library code resident even
 // if the SDR host unloads this module; do not leave dangling type callbacks.
 auto* library=dlopen("libgtk-3.so.0",RTLD_NOW|RTLD_LOCAL|RTLD_NODELETE);if(!library)return false;bool ok=gtk_init_check(nullptr,nullptr);dlclose(library);return ok;
}();nativeInitialized=ready;return ready;}
void pollLinuxNative(){if(nativeInitialized)for(int i=0;i<8&&gtk_events_pending();i++)gtk_main_iteration_do(FALSE);}
void linuxOpenUrl(const std::string& url){if(!linuxNativeReady()||url.rfind("https://",0)!=0)return;GError* error=nullptr;gtk_show_uri_on_window(nullptr,url.c_str(),GDK_CURRENT_TIME,&error);if(error)g_error_free(error);}
std::optional<std::string> linuxSavePath(const std::string& title,const std::string& current,const char* pattern){
 if(!linuxNativeReady())return {};
 auto* picker=gtk_file_chooser_dialog_new(title.c_str(),nullptr,GTK_FILE_CHOOSER_ACTION_SAVE,"Cancel",GTK_RESPONSE_CANCEL,"Save",GTK_RESPONSE_ACCEPT,nullptr);
 auto* chooser=GTK_FILE_CHOOSER(picker);gtk_file_chooser_set_do_overwrite_confirmation(chooser,TRUE);
 auto path=std::filesystem::path(current);if(path.is_absolute())gtk_file_chooser_set_current_folder(chooser,path.parent_path().c_str());gtk_file_chooser_set_current_name(chooser,path.filename().c_str());
 auto* filter=gtk_file_filter_new();gtk_file_filter_add_pattern(filter,pattern);gtk_file_filter_set_name(filter,pattern);gtk_file_chooser_add_filter(chooser,filter);
 std::optional<std::string> selected;if(gtk_dialog_run(GTK_DIALOG(picker))==GTK_RESPONSE_ACCEPT){char* file=gtk_file_chooser_get_filename(chooser);if(file){selected=file;g_free(file);}}
 gtk_widget_destroy(picker);pollLinuxNative();return selected;
}
namespace {
std::vector<unsigned char> glyphPixels(const std::string& text){
 auto* surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,64,64);auto* cr=cairo_create(surface);auto* layout=pango_cairo_create_layout(cr);
 auto* font=pango_font_description_from_string("Noto Color Emoji 38");pango_layout_set_font_description(layout,font);pango_layout_set_text(layout,text.c_str(),-1);int width=0,height=0;pango_layout_get_pixel_size(layout,&width,&height);cairo_move_to(cr,(64-width)/2.,(64-height)/2.);pango_cairo_show_layout(cr,layout);
 cairo_surface_flush(surface);auto* bytes=cairo_image_surface_get_data(surface);int stride=cairo_image_surface_get_stride(surface);std::vector<unsigned char> rgba(64*64*4);
 for(int y=0;y<64;y++)for(int x=0;x<64;x++){auto* p=bytes+y*stride+x*4;auto* q=rgba.data()+size_t(y*64+x)*4;q[0]=p[3]?std::min(255u,unsigned(p[2])*255/p[3]):0;q[1]=p[3]?std::min(255u,unsigned(p[1])*255/p[3]):0;q[2]=p[3]?std::min(255u,unsigned(p[0])*255/p[3]):0;q[3]=p[3];}
 pango_font_description_free(font);g_object_unref(layout);cairo_destroy(cr);cairo_surface_destroy(surface);return rgba;
}
std::string utf8(uint32_t cp){std::string s;for(int shift:{18,12,6,0})s+=char(shift==18?0xf0|(cp>>shift):0x80|((cp>>shift)&63));return s;}
}
std::vector<unsigned char> homeIconPixels(){return glyphPixels("🏠");}
std::vector<unsigned char> trophyIconPixels(){return glyphPixels("🏆");}
std::vector<unsigned char> flagIconPixels(const std::string& code){if(code.empty())return glyphPixels("🏳️");if(code.size()!=2||code[0]<'A'||code[0]>'Z'||code[1]<'A'||code[1]>'Z')return {};return glyphPixels(utf8(0x1f1e6+code[0]-'A')+utf8(0x1f1e6+code[1]-'A'));}
void openFeedbackProfile(){linuxOpenUrl("https://x.com/FoxOnTheRunTr");}
void openCallsignLookup(const std::string& call,int provider){linuxOpenUrl(callsignLookupUrl(call,provider));}
void openMapCredit(int index){const char* urls[]={"https://www.openstreetmap.org/copyright","https://www.hotosm.org","https://www.openstreetmap.fr","https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer"};if(index>=0&&index<4)linuxOpenUrl(urls[index]);}
std::optional<std::string> chooseCsvLog(const std::string& path){return linuxSavePath("Reception CSV log",path,"*.csv");}
bool playAlertSound(const std::string& name){if(name=="None")return true;if(!linuxNativeReady())return false;auto* display=gdk_display_get_default();if(!display)return false;gdk_display_beep(display);return true;}
}
