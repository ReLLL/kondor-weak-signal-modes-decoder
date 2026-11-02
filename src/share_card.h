// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
#include <functional>
namespace dx {
inline constexpr int shareWidth=2400,shareHeight=1350;
struct ShareBackground {const unsigned char* data;size_t size;};
ShareBackground shareBackground();
struct ShareCard {
 int64_t capturedUtc=0,oldestUtc=0;Stats stats;ActivityBearing direction;
 bool miles=false,history=false,includeReceiver=true;
 std::string version,receiverGrid,receiverCountry,modes,bearingScope,operatorCall="MY STATION";
};
struct ShareColor {unsigned char r,g,b,a=255;};
struct ShareMark {
 enum class Kind {Text,Rectangle,Line,Ellipse,Trophy,Background};Kind kind=Kind::Text;
 float x=0,y=0,w=0,h=0,size=0;ShareColor color{255,255,255};
 std::string text;bool bold=false,display=false;
 std::string accent;ShareColor accentColor{239,193,115};
};
std::vector<ShareMark> shareComposition(const ShareCard& card);
std::string shareCaption(const ShareCard& card);
bool sameShareContent(const ShareCard& a,const ShareCard& b);
enum class ShareService {X,Facebook,Instagram};
std::string shareServiceUrl(ShareService service,const ShareCard& card);
struct ShareImage {
 std::vector<unsigned char> rgba,png;std::string error;
 bool valid()const{return rgba.size()==size_t(shareWidth)*shareHeight*4&&!png.empty();}
};
// Native rasterizers run on a bounded, one-shot worker; no radio or database work.
ShareImage renderShareCard(const ShareCard& card);
bool copyShareImage(const ShareImage& image,const std::string& caption);
// Empty means Cancel, a nonempty value is the saved path or an error beginning "Error:".
std::string saveShareImage(const ShareImage& image,int64_t capturedUtc);
void openShareService(ShareService service,const ShareCard& card);
}
