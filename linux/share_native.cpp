// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "native.h"
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <fstream>
#include <cstring>
namespace dx {
namespace {struct ClipboardContent{std::vector<unsigned char> png;std::string caption;};ClipboardContent* ownedClipboard=nullptr;}
ShareImage renderShareCard(const ShareCard& card){
 ShareImage result;auto* surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,shareWidth,shareHeight);auto* cr=cairo_create(surface);
 for(const auto& m:shareComposition(card)){
  cairo_set_source_rgba(cr,m.color.r/255.,m.color.g/255.,m.color.b/255.,m.color.a/255.);
  switch(m.kind){
   case ShareMark::Kind::Background:{
    struct Input{ShareBackground png;size_t offset=0;} input{shareBackground()};
    auto read=[](void* value,unsigned char* out,unsigned int count){auto& in=*static_cast<Input*>(value);if(count>in.png.size-in.offset)return CAIRO_STATUS_READ_ERROR;std::memcpy(out,in.png.data+in.offset,count);in.offset+=count;return CAIRO_STATUS_SUCCESS;};
    auto* photo=cairo_image_surface_create_from_png_stream(read,&input);
    if(cairo_surface_status(photo)==CAIRO_STATUS_SUCCESS){cairo_save(cr);cairo_translate(cr,m.x,m.y);cairo_scale(cr,m.w/cairo_image_surface_get_width(photo),m.h/cairo_image_surface_get_height(photo));cairo_set_source_surface(cr,photo,0,0);cairo_paint(cr);cairo_restore(cr);}cairo_surface_destroy(photo);break;
   }
   case ShareMark::Kind::Rectangle:cairo_rectangle(cr,m.x,m.y,m.w,m.h);cairo_fill(cr);break;
   case ShareMark::Kind::Line:cairo_set_line_width(cr,m.size);cairo_move_to(cr,m.x,m.y);cairo_line_to(cr,m.w,m.h);cairo_stroke(cr);break;
   case ShareMark::Kind::Ellipse:cairo_new_path(cr);cairo_save(cr);cairo_translate(cr,m.x+m.w/2,m.y+m.h/2);cairo_scale(cr,m.w/2,m.h/2);cairo_arc(cr,0,0,1,0,6.28318530718);cairo_restore(cr);cairo_set_line_width(cr,m.size);cairo_stroke(cr);break;
   case ShareMark::Kind::Trophy:
   case ShareMark::Kind::Text:{
    auto* layout=pango_cairo_create_layout(cr);auto* face=pango_font_description_new();pango_font_description_set_family(face,m.kind==ShareMark::Kind::Trophy?"Noto Color Emoji":"DejaVu Sans");pango_font_description_set_weight(face,m.bold?PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL);float size=m.kind==ShareMark::Kind::Trophy?m.h:m.size;
    auto value=m.text+m.accent;pango_layout_set_text(layout,m.kind==ShareMark::Kind::Trophy?"🏆":value.c_str(),-1);int width=0,height=0;
    if(!m.accent.empty()){auto* attributes=pango_attr_list_new();auto* accent=pango_attr_foreground_new(m.accentColor.r*257,m.accentColor.g*257,m.accentColor.b*257);accent->start_index=guint(m.text.size());accent->end_index=G_MAXUINT;pango_attr_list_insert(attributes,accent);pango_layout_set_attributes(layout,attributes);pango_attr_list_unref(attributes);}
    const float minimum=size*.65f;do{pango_font_description_set_absolute_size(face,size*PANGO_SCALE);pango_layout_set_font_description(layout,face);pango_layout_get_pixel_size(layout,&width,&height);if(width<=m.w||size<=minimum)break;size-=1;}while(true);
    pango_layout_set_width(layout,int(m.w*PANGO_SCALE));pango_layout_set_ellipsize(layout,PANGO_ELLIPSIZE_END);pango_layout_set_single_paragraph_mode(layout,TRUE);cairo_move_to(cr,m.x,m.y);pango_cairo_show_layout(cr,layout);pango_font_description_free(face);g_object_unref(layout);break;
   }
  }
 }
 cairo_destroy(cr);cairo_surface_flush(surface);auto* bytes=cairo_image_surface_get_data(surface);int stride=cairo_image_surface_get_stride(surface);
 result.rgba.resize(size_t(shareWidth)*shareHeight*4);for(int y=0;y<shareHeight;y++)for(int x=0;x<shareWidth;x++){auto* p=bytes+y*stride+x*4;auto* q=result.rgba.data()+size_t(y*shareWidth+x)*4;q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=p[3];}
 auto write=[](void* out,const unsigned char* data,unsigned int count){auto& png=*static_cast<std::vector<unsigned char>*>(out);png.insert(png.end(),data,data+count);return CAIRO_STATUS_SUCCESS;};
 if(cairo_surface_write_to_png_stream(surface,write,&result.png)!=CAIRO_STATUS_SUCCESS)result.error="Could not encode the share image.";cairo_surface_destroy(surface);if(!result.valid()&&result.error.empty())result.error="Could not render the share image.";return result;
}
bool copyShareImage(const ShareImage& image,const std::string& caption){
 if(!image.valid()||!linuxNativeReady())return false;
 auto* content=new ClipboardContent{image.png,caption};
 GtkTargetEntry targets[]={{const_cast<char*>("image/png"),0,0},{const_cast<char*>("UTF8_STRING"),0,1},{const_cast<char*>("text/plain;charset=utf-8"),0,1}};
 auto get=[](GtkClipboard*,GtkSelectionData* data,guint info,gpointer value){auto& c=*static_cast<ClipboardContent*>(value);if(info==0)gtk_selection_data_set(data,gdk_atom_intern_static_string("image/png"),8,c.png.data(),int(c.png.size()));else gtk_selection_data_set_text(data,c.caption.c_str(),-1);};
 auto clear=[](GtkClipboard*,gpointer value){if(ownedClipboard==value)ownedClipboard=nullptr;delete static_cast<ClipboardContent*>(value);};auto* board=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
 if(!gtk_clipboard_set_with_data(board,targets,3,get,clear,content)){delete content;return false;}ownedClipboard=content;gtk_clipboard_set_can_store(board,nullptr,0);gtk_clipboard_store(board);pollLinuxNative();return true;
}
void releaseLinuxShareClipboard(){
 // Replace our provider synchronously with GTK's own image provider before
 // the module unloads. Clipboard content survives without callbacks into us.
 if(!ownedClipboard||!linuxNativeReady())return;pollLinuxNative();if(!ownedClipboard)return;
 auto* stream=g_memory_input_stream_new_from_data(ownedClipboard->png.data(),gssize(ownedClipboard->png.size()),nullptr);
 auto* pixbuf=gdk_pixbuf_new_from_stream(stream,nullptr,nullptr);g_object_unref(stream);auto* board=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
 if(pixbuf){gtk_clipboard_set_image(board,pixbuf);g_object_unref(pixbuf);}else{auto caption=ownedClipboard->caption;gtk_clipboard_set_text(board,caption.c_str(),-1);}
 gtk_clipboard_store(board);
}
std::string saveShareImage(const ShareImage& image,int64_t utc){if(!image.valid())return "Error: No image is ready.";auto path=linuxSavePath("Save reception highlights","Kondor-reception-"+std::to_string(utc)+".png","*.png");if(!path)return {};if(std::filesystem::path(*path).extension()!=".png")*path+=".png";std::ofstream out(*path,std::ios::binary);out.write(reinterpret_cast<const char*>(image.png.data()),std::streamsize(image.png.size()));out.close();return out?*path:"Error: Could not save the PNG image.";}
void openShareService(ShareService service,const ShareCard& card){linuxOpenUrl(shareServiceUrl(service,card));}
}
