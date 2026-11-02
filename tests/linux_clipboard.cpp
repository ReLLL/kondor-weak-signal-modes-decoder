// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
// Runs only inside its own Xvfb display; never replaces the desktop clipboard.
#include "native.h"
#include <gtk/gtk.h>
#include <iostream>
#define CHECK(value) do{if(!(value)){std::cerr<<"Clipboard check failed: "<<__LINE__<<'\n';return 1;}}while(0)
int main(){
 dx::pollLinuxNative();CHECK(gdk_display_get_default()==nullptr);
 CHECK(dx::linuxNativeReady());dx::ShareCard card;card.capturedUtc=1793610000;auto image=dx::renderShareCard(card);CHECK(image.valid());
 const std::string caption="Isolated Kondor clipboard test";CHECK(dx::copyShareImage(image,caption));auto* board=gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
 char* text=gtk_clipboard_wait_for_text(board);CHECK(text&&caption==text);g_free(text);
 auto* data=gtk_clipboard_wait_for_contents(board,gdk_atom_intern_static_string("image/png"));CHECK(data&&gtk_selection_data_get_length(data)==int(image.png.size()));gtk_selection_data_free(data);
 dx::releaseLinuxShareClipboard();auto* pixbuf=gtk_clipboard_wait_for_image(board);CHECK(pixbuf&&gdk_pixbuf_get_width(pixbuf)==dx::shareWidth&&gdk_pixbuf_get_height(pixbuf)==dx::shareHeight);g_object_unref(pixbuf);
 // A later copy from somewhere else must not be replaced during cleanup.
 gtk_clipboard_set_text(board,"Another clipboard owner",-1);dx::releaseLinuxShareClipboard();text=gtk_clipboard_wait_for_text(board);CHECK(text&&std::string(text)=="Another clipboard owner");g_free(text);
 CHECK(dx::copyShareImage(image,caption));dx::releaseLinuxShareClipboard();dx::releaseLinuxShareClipboard();
 std::cout<<"PNG/text clipboard round-trip, unload-safe provider handoff and unrelated-owner preservation passed\n";
}
