// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#import <AppKit/AppKit.h>
#include "share_card.h"
#include <cstring>
namespace dx {
namespace {
NSColor* color(ShareColor c){return [NSColor colorWithSRGBRed:c.r/255. green:c.g/255. blue:c.b/255. alpha:c.a/255.];}
NSString* string(const std::string& value){return [NSString stringWithUTF8String:value.c_str()]?:@"";}
NSFont* font(float size,bool bold,bool display){return [NSFont fontWithName:display?@"AvenirNext-DemiBold":bold?@"AvenirNext-DemiBold":@"AvenirNext-Regular" size:size]?:[NSFont systemFontOfSize:size weight:bold?NSFontWeightSemibold:NSFontWeightRegular];}
}
ShareImage renderShareCard(const ShareCard& card){@autoreleasepool{
 ShareImage result;
 NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil pixelsWide:shareWidth pixelsHigh:shareHeight bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:0 bytesPerRow:shareWidth*4 bitsPerPixel:32];
 if(!bitmap){result.error="Could not create the share image.";return result;}
 [NSGraphicsContext saveGraphicsState];[NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
 for(const auto& m:shareComposition(card)){
  [color(m.color) set];auto rect=NSMakeRect(m.x,shareHeight-m.y-m.h,m.w,m.h);
  switch(m.kind){
   case ShareMark::Kind::Background:{auto bytes=shareBackground();NSData* data=[NSData dataWithBytesNoCopy:const_cast<unsigned char*>(bytes.data) length:bytes.size freeWhenDone:NO];NSImage* photo=[[[NSImage alloc] initWithData:data] autorelease];if(photo)[photo drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1];break;}
   case ShareMark::Kind::Rectangle:NSRectFillUsingOperation(rect,NSCompositingOperationSourceOver);break;
   case ShareMark::Kind::Line:{auto* path=[NSBezierPath bezierPath];path.lineWidth=m.size;[path moveToPoint:NSMakePoint(m.x,shareHeight-m.y)];[path lineToPoint:NSMakePoint(m.w,shareHeight-m.h)];[path stroke];break;}
   case ShareMark::Kind::Ellipse:{auto* path=[NSBezierPath bezierPathWithOvalInRect:rect];path.lineWidth=m.size;[path stroke];break;}
   case ShareMark::Kind::Trophy:{[@"🏆" drawInRect:rect withAttributes:@{NSFontAttributeName:[NSFont fontWithName:@"Apple Color Emoji" size:m.h]?:font(m.h,true,false)}];break;}
   case ShareMark::Kind::Text:{
    NSString* value=string(m.text+m.accent);float size=m.size;NSFont* face=font(size,m.bold,m.display);
    while(size>m.size*.65f&&[value sizeWithAttributes:@{NSFontAttributeName:face}].width>m.w){size-=1;face=font(size,m.bold,m.display);}
    auto* style=[[[NSMutableParagraphStyle alloc] init] autorelease];style.lineBreakMode=NSLineBreakByTruncatingTail;
    auto* rich=[[[NSMutableAttributedString alloc] initWithString:value attributes:@{NSFontAttributeName:face,NSForegroundColorAttributeName:color(m.color),NSParagraphStyleAttributeName:style}] autorelease];
    if(!m.accent.empty())[rich addAttribute:NSForegroundColorAttributeName value:color(m.accentColor) range:NSMakeRange(string(m.text).length,string(m.accent).length)];
    [rich drawInRect:rect];break;
   }
  }
 }
 [NSGraphicsContext restoreGraphicsState];
 result.rgba.assign(bitmap.bitmapData,bitmap.bitmapData+size_t(shareWidth)*shareHeight*4);
 NSData* data=[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
 if(data.length)result.png.assign(static_cast<const unsigned char*>(data.bytes),static_cast<const unsigned char*>(data.bytes)+data.length);
 [bitmap release];if(!result.valid())result.error="Could not encode the share image.";return result;
}}
bool copyShareImage(const ShareImage& image,const std::string& caption){@autoreleasepool{
 if(!image.valid())return false;auto* item=[[[NSPasteboardItem alloc] init] autorelease];
 NSData* png=[NSData dataWithBytes:image.png.data() length:image.png.size()];
 NSImage* picture=[[[NSImage alloc] initWithData:png] autorelease];if(!picture)return false;
 [item setData:png forType:NSPasteboardTypePNG];if(auto* tiff=picture.TIFFRepresentation)[item setData:tiff forType:NSPasteboardTypeTIFF];
 [item setString:string(caption) forType:NSPasteboardTypeString];auto* board=[NSPasteboard generalPasteboard];[board clearContents];return [board writeObjects:@[item]];
}}
std::string saveShareImage(const ShareImage& image,int64_t utc){@autoreleasepool{
 if(!image.valid())return "Error: No image is ready.";
 auto* panel=[NSSavePanel savePanel];panel.title=@"Save reception highlights";panel.canCreateDirectories=YES;panel.allowedFileTypes=@[@"png"];
 panel.nameFieldStringValue=string("Kondor-reception-"+std::to_string(utc)+".png");
 if([panel runModal]!=NSModalResponseOK)return {};
 NSData* png=[NSData dataWithBytes:image.png.data() length:image.png.size()];NSError* error=nil;
 if(![png writeToURL:panel.URL options:NSDataWritingAtomic error:&error])return "Error: Could not save the PNG image.";
 return std::string(panel.URL.path.UTF8String);
}}
void openShareService(ShareService service,const ShareCard& card){@autoreleasepool{[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:string(shareServiceUrl(service,card))]];}}
}
