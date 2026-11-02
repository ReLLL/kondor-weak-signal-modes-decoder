// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin.h"
#include <core.h>
#include <utils/opengl_include_code.h>
#include <algorithm>
#include <chrono>
#include <limits>
void DXNotebook::renderShare(){
 shareRenderPending_=true;
 if(shareFuture_.valid())return;
 shareRenderPending_=false;shareStatus_.clear();
 auto card=shareCard_;shareFuture_=std::async(std::launch::async,[card]{return dx::renderShareCard(card);});
}
void DXNotebook::prepareShare(const dx::Snapshot& s,bool force){
 shareRefreshAt_=ImGui::GetTime()+3;
 auto previous=std::move(shareCard_);shareCard_={};shareCard_.includeReceiver=previous.includeReceiver;
 shareCard_.capturedUtc=time(nullptr);shareCard_.stats=s.stats;shareCard_.direction=s.direction;
 shareCard_.oldestUtc=s.retention.firstUtc;shareCard_.history=s.statsHistory||s.historyAll;
 shareCard_.version=DX_VERSION;shareCard_.miles=distanceMiles_;
 shareCard_.operatorCall=shareCallsign_[0]?shareCallsign_:"MY STATION";
#if DX_HOST_BROWN
 core::configManager.acquire();shareCard_.receiverGrid=core::configManager.conf.value("operatorLocation","");core::configManager.release();
#else
 shareCard_.receiverGrid=receiverGrid_;
#endif
 // Use geography, not the operator callsign's prefix, for the receiver country.
 if(shareCard_.receiverGrid==previous.receiverGrid)shareCard_.receiverCountry=previous.receiverCountry;
 else if(auto point=dx::gridPoint(shareCard_.receiverGrid))for(const auto& country:mapCountries_){bool found=false;for(const auto& polygon:country.polygons)if(dx::insidePolygon(*point,polygon)){found=true;break;}if(found){shareCard_.receiverCountry=country.name;break;}}
 for(const auto& mode:dx::modeSpecs())if(modeEnabled(std::string(mode.name))){if(!shareCard_.modes.empty())shareCard_.modes+=", ";shareCard_.modes+=mode.name;}
 const char* times[]={"Last minute","Last 3 minutes","Last 5 minutes","Last 10 minutes","Last 15 minutes","Last hour","Last 6 hours","Last 24 hours","Last 7 days","Last month","Last year","All","Custom dates"};
 shareCard_.bearingScope=std::string(times[std::clamp(tableTime_,0,12)])+" / "+(tableFilter_.band.empty()?"all bands":tableFilter_.band)+" / "+(tableFilter_.mode.empty()?"all modes":tableFilter_.mode);
 if(tableTime_==12)shareCard_.bearingScope+=" / "+dx::localDateTime(tableQuery_.since)+" to "+dx::localDateTime(tableQuery_.until);
 if(!force&&dx::sameShareContent(previous,shareCard_)){shareCard_=std::move(previous);return;}
 renderShare();
}
void DXNotebook::shareWindow(const dx::Snapshot& snapshot){
 auto* vp=ImGui::GetMainViewport();
 const float maximumWidth=std::max(300.f,vp->WorkSize.x-32),minimumWidth=std::min(460.f,maximumWidth);
 ImGui::SetNextWindowSize({std::clamp(shareWindowWidth_,minimumWidth,maximumWidth),0},ImGuiCond_Always);
 ImGui::SetNextWindowSizeConstraints({minimumWidth,0},{maximumWidth,std::max(280.f,vp->WorkSize.y-32)});
 bool visible=true;if(!ImGui::BeginPopupModal("Share reception highlights",&visible))return;popup_=true;
 if(!shareFuture_.valid()&&ImGui::GetTime()>=shareRefreshAt_)prepareShare(snapshot,false);
 if(shareFuture_.valid()&&shareFuture_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
  dx::ShareImage rendered;try{rendered=shareFuture_.get();}catch(const std::exception&){rendered.error="Could not generate the image. Try Refresh.";}
  if(shareRenderPending_)renderShare();
  else {shareImage_=std::move(rendered);
  if(shareImage_.valid()){
   if(shareTexture_){glDeleteTextures(1,&shareTexture_);shareTexture_=0;}
   GLint old=0,row=0,alignment=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&old);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
   glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glGenTextures(1,&shareTexture_);glBindTexture(GL_TEXTURE_2D,shareTexture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,dx::shareWidth,dx::shareHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,shareImage_.rgba.data());glBindTexture(GL_TEXTURE_2D,GLuint(old));glPixelStorei(GL_UNPACK_ROW_LENGTH,row);glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);
  }else {if(shareTexture_){glDeleteTextures(1,&shareTexture_);shareTexture_=0;}shareStatus_=shareImage_.error;}}
 }
 const char* heading="Share your reception highlights with others on social media";const float headingSize=ImGui::GetFontSize()*1.5f,headingWidth=ImGui::GetContentRegionAvail().x;
 auto headingBounds=ImGui::GetFont()->CalcTextSizeA(headingSize,std::numeric_limits<float>::max(),headingWidth,heading);
 ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),headingSize,ImGui::GetCursorScreenPos(),ImGui::GetColorU32(ImGuiCol_Text),heading,nullptr,headingWidth);ImGui::Dummy({std::min(headingWidth,headingBounds.x),headingBounds.y});
 const bool busy=shareFuture_.valid();
 if(ImGui::Button("Refresh snapshot"))prepareShare(snapshot);
 if(ImGui::IsItemHovered())ImGui::SetTooltip("Checks current stats every 3 seconds while open. Redraws only when displayed data changes, with at most one render running. Click to refresh now.");
 ImGui::SameLine();if(ImGui::Checkbox("Include receiver location",&shareCard_.includeReceiver))renderShare();
 const float contentWidth=ImGui::GetContentRegionAvail().x,callWidth=std::min(210.f,contentWidth*.43f);
 const float labelsWidth=ImGui::CalcTextSize("Callsign").x+ImGui::CalcTextSize("Distance units").x;
 ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Callsign");ImGui::SameLine();ImGui::SetNextItemWidth(callWidth);
 if(ImGui::InputText("##share_callsign",shareCallsign_,sizeof(shareCallsign_))){shareCard_.operatorCall=shareCallsign_[0]?shareCallsign_:"MY STATION";engine_->settings({{"share_callsign",std::string(shareCallsign_)}});renderShare();}
 if(ImGui::IsItemHovered())ImGui::SetTooltip("Shown in gold in the image heading. Your text is remembered exactly, including spaces. Empty uses MY STATION.");
 if(contentWidth>=labelsWidth+callWidth+175+ImGui::GetStyle().ItemSpacing.x*3)ImGui::SameLine();
 ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Distance units");ImGui::SameLine();
 ImGui::SetNextItemWidth(175);int units=distanceMiles_?1:0;if(ImGui::Combo("##share_distance_units",&units,"Kilometers (km)\0Miles (mi)\0")){distanceMiles_=units==1;shareCard_.miles=distanceMiles_;engine_->settings({{"distance_miles",distanceMiles_}});renderShare();}
 // Intrinsic preview height, independent of the previous window height: auto-fit
 // removes blank space without creating a resize/render feedback loop.
 const float available=std::max(32.f,vp->WorkSize.y-64-ImGui::GetTextLineHeightWithSpacing()*9-ImGui::GetFrameHeightWithSpacing()*5);
 const float width=std::min(ImGui::GetContentRegionAvail().x,available*dx::shareWidth/dx::shareHeight),height=width*dx::shareHeight/dx::shareWidth;
 if(shareTexture_)ImGui::Image(reinterpret_cast<ImTextureID>(uintptr_t(shareTexture_)),{width,height});else{ImGui::BeginChild("share_generating",{width,height},true);ImGui::TextWrapped("%s",busy?"Creating your image...":shareImage_.error.c_str());ImGui::EndChild();}
 ImGui::BeginDisabled(!shareTexture_||shareFuture_.valid());
 const float lineWidth=ImGui::GetContentRegionAvail().x;float used=0;auto button=[&](const char* title){float w=ImGui::CalcTextSize(title).x+ImGui::GetStyle().FramePadding.x*2,gap=ImGui::GetStyle().ItemSpacing.x;if(used&&used+gap+w<=lineWidth){ImGui::SameLine();used+=gap;}else used=0;used+=w;return ImGui::Button(title);};
 if(button("X / Twitter")){if(dx::copyShareImage(shareImage_,dx::shareCaption(shareCard_))){dx::openShareService(dx::ShareService::X,shareCard_);shareStatus_="Image copied. Paste it into the X draft, review, then post.";}else shareStatus_="Could not copy the image. Try Save PNG instead.";}
 if(button("Facebook")){if(dx::copyShareImage(shareImage_,dx::shareCaption(shareCard_))){dx::openShareService(dx::ShareService::Facebook,shareCard_);shareStatus_="Image copied. Create a Facebook post and paste it, or attach a saved PNG.";}else shareStatus_="Could not copy the image. Try Save PNG instead.";}
 if(button("Instagram")){auto saved=dx::saveShareImage(shareImage_,shareCard_.capturedUtc);if(saved.rfind("Error:",0)==0)shareStatus_=saved;else if(!saved.empty()){ImGui::SetClipboardText(dx::shareCaption(shareCard_).c_str());dx::openShareService(dx::ShareService::Instagram,shareCard_);shareStatus_="PNG saved; caption copied. In Instagram, choose Create > Post and select that image.";}}
 if(button("Copy text")){ImGui::SetClipboardText(dx::shareCaption(shareCard_).c_str());shareStatus_="Caption copied. Paste it with your image.";}
 if(button("Copy image"))shareStatus_=dx::copyShareImage(shareImage_,dx::shareCaption(shareCard_))?"Image and suggested caption copied.":"Could not copy the image. Try Save PNG instead.";
 if(button("Save PNG...")){auto saved=dx::saveShareImage(shareImage_,shareCard_.capturedUtc);if(!saved.empty())shareStatus_=saved.rfind("Error:",0)==0?saved:"Saved: "+saved;}
 ImGui::EndDisabled();ImGui::TextWrapped("X/Facebook: paste the copied image into your draft. Instagram: choose the saved PNG and paste the copied caption.");
 ImGui::TextWrapped("%s",shareStatus_.empty()?"Nothing is posted automatically. Reception continues while you share.":shareStatus_.c_str());
 if(ImGui::Button("Close"))ImGui::CloseCurrentPopup();shareWindowWidth_=ImGui::GetWindowWidth();ImGui::EndPopup();
}
