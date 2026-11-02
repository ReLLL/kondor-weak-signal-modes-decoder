// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#import <AppKit/AppKit.h>
#include <optional>
#include <string>
#include <cstring>
#include "alerts.h"
namespace dx {
namespace {
std::vector<unsigned char> emojiPixels(NSString* glyph){@autoreleasepool{
 NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:64 pixelsHigh:64 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:0 bytesPerRow:256 bitsPerPixel:32];if(!bitmap)return {};
 std::memset(bitmap.bitmapData,0,64*256);[NSGraphicsContext saveGraphicsState];[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
 NSFont* font=[NSFont fontWithName:@"Apple Color Emoji" size:54]?:[NSFont systemFontOfSize:54];NSDictionary* attributes=@{NSFontAttributeName:font};NSSize size=[glyph sizeWithAttributes:attributes];[glyph drawAtPoint:NSMakePoint((64-size.width)/2,(64-size.height)/2) withAttributes:attributes];[NSGraphicsContext restoreGraphicsState];
 std::vector<unsigned char> pixels(bitmap.bitmapData,bitmap.bitmapData+64*256);[bitmap release];
 // AppKit renders premultiplied alpha; ImGui's blend expects straight RGBA.
 for(size_t i=0;i<pixels.size();i+=4)if(pixels[i+3])for(int c=0;c<3;c++)pixels[i+c]=uint8_t(std::min(255u,unsigned(pixels[i+c])*255/unsigned(pixels[i+3])));return pixels;
}}
}
std::vector<unsigned char> homeIconPixels(){return emojiPixels(@"🏠");}
std::vector<unsigned char> trophyIconPixels(){return emojiPixels(@"🏆");}
std::vector<unsigned char> flagIconPixels(const std::string& code){@autoreleasepool{
 if(code.empty())return emojiPixels(@"🏳️");
 if(code.size()!=2||code[0]<'A'||code[0]>'Z'||code[1]<'A'||code[1]>'Z')return {};
 unichar chars[]={0xD83C,unichar(0xDDE6+code[0]-'A'),0xD83C,unichar(0xDDE6+code[1]-'A')};return emojiPixels([NSString stringWithCharacters:chars length:4]);
}}
bool playAlertSound(const std::string& name){@autoreleasepool{
 if(name=="None")return true;bool known=false;for(auto preset:alertSoundNames)if(name==preset)known=true;if(!known)return false;
 NSSound* sound=[NSSound soundNamed:[NSString stringWithUTF8String:name.c_str()]];if(!sound)return false;[sound stop];return [sound play];
}}
void openMapCredit(int index){
 const char* urls[]={"https://www.openstreetmap.org/copyright","https://www.hotosm.org","https://www.openstreetmap.fr","https://server.arcgisonline.com/ArcGIS/rest/services/World_Street_Map/MapServer"};
 if(index<0||index>3)return;@autoreleasepool{NSURL* url=[NSURL URLWithString:[NSString stringWithUTF8String:urls[index]]];[[NSWorkspace sharedWorkspace] openURL:url];}
}
void openCallsignLookup(const std::string& call,int provider){auto link=callsignLookupUrl(call,provider);if(link.empty())return;@autoreleasepool{
 NSURL* url=[NSURL URLWithString:[NSString stringWithUTF8String:link.c_str()]];[[NSWorkspace sharedWorkspace] openURL:url];
}}
void openFeedbackProfile(){@autoreleasepool{[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://x.com/FoxOnTheRunTr"]];}}
std::optional<std::string> chooseCsvLog(const std::string& current){@autoreleasepool{
 NSSavePanel* panel=[NSSavePanel savePanel];panel.title=@"Reception CSV log";panel.canCreateDirectories=YES;
 NSString* path=[NSString stringWithUTF8String:current.c_str()];panel.directoryURL=[NSURL fileURLWithPath:[path stringByDeletingLastPathComponent]];panel.nameFieldStringValue=[path lastPathComponent];panel.allowedFileTypes=@[@"csv"];
 if([panel runModal]!=NSModalResponseOK)return {};return std::string(panel.URL.path.UTF8String);
}}
}
