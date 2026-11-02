// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <imgui_internal.h>
#include <algorithm>
#include <string_view>
namespace dx {
inline bool containsUnresolvedCall(std::string_view value){return value.find("<0:")!=std::string_view::npos||value.find("<1:")!=std::string_view::npos||value.find("<2:")!=std::string_view::npos||value.find("<...>")!=std::string_view::npos;}
inline const char* unresolvedCallHelp(){return "The full callsign was not transmitted here. <0:n>, <1:n> and <2:n> are 10-, 12- and 22-bit callsign hashes; <...> is an unresolved placeholder. These are not country codes.\n\nKondor compares the hash with full sender callsigns heard during the preceding 12 minutes, excluding low-confidence decodes. It accepts exactly one matching callsign. At this reception, no single usable match was available: the full call may not have been heard yet, may be outside that window, or multiple calls may share the hash.\n\nHearing the full callsign first can resolve later hashed receptions. A hash cannot uniquely reconstruct a callsign by itself. Keeping it unresolved avoids inventing an identity, country or location. Original message text is preserved; later receptions do not rewrite this saved message. An unresolved target can be legitimate and does not by itself make the known sender or its grid invalid.";}
inline void unresolvedCallTooltip(std::string_view value){if(!containsUnresolvedCall(value)||!ImGui::IsItemHovered())return;ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*34);ImGui::TextUnformatted("Unresolved callsign hash");ImGui::Separator();ImGui::TextUnformatted(unresolvedCallHelp());ImGui::PopTextWrapPos();ImGui::EndTooltip();}
inline const char* tableColumnHelp(std::string_view name){
 if(name=="Sender")return "Transmitting station's decoded or resolved callsign. Bracketed numeric tokens are unresolved callsign hashes, not country codes. Free text can have no identified sender.";
 if(name=="Country")return "Country or DXCC entity inferred from the sender's callsign using the callsign database. FT8/FT4 does not send a separate country field. This is not proof of the transmitter's exact location; portable stations can operate elsewhere.";
 if(name=="Grid")return "Maidenhead locator used for this reception. A received grid describes an area; positions and distances use its center. A cached grid came from an earlier reception of the same sender. Blank means no received or usable cached grid is available.";
 if(name=="SNR dB")return "Signal-to-noise ratio estimated by the decoder at your receiver, in decibels. Higher values indicate a stronger signal relative to noise. This is not the dBFS input level, transmitter power or decoder confidence.";
 if(name=="Distance")return "Great-circle distance from your home grid to the station's grid center, not between the QSO parties. * marks a cached grid; [C] marks a coarse nearest-country-grid estimate, not a located station. The trophy marks the farthest accepted station in the statistics scope; country estimates do not qualify.";
 if(name=="Msg age")return "Time since this reception. Unique stations shows the latest matching reception from each sender; repeats update its age. Up to 600 seconds is shown as seconds ago, then local date/time. Sorting always uses the underlying reception timestamp.";
 if(name=="Mod")return "Digital mode used to decode this reception, such as FT8, FT4 or WSPR. In Unique stations this is the sender's latest matching reception mode.";
 if(name=="Band")return "Amateur radio band derived from the reception's dial frequency, such as 40m or 20m. This is a wavelength label, not the digital mode.";
 if(name=="Dial MHz")return "Dial frequency of the decoding channel in megahertz. This is the mode's receive preset, which can differ from the host's spectrum center frequency. The decoded audio offset is listed separately.";
 if(name=="Audio Hz")return "Decoded signal's audio-frequency offset from the decoding channel's dial frequency, in hertz. This is frequency, not signal strength or an audio volume level.";
 if(name=="DT s")return "Decoder's estimated timing offset from the expected transmission frame, in seconds. Clock differences and path delay can contribute. It is not a direct propagation-delay measurement or the host's manual Time correction setting.";
 if(name=="Bearing")return "Initial great-circle bearing from your home location, clockwise from true north: 0 north, 90 east, 180 south, 270 west. [C] means a coarse country estimate. This is a geographic direction, not a measured arrival direction.";
 if(name=="Count")return "Number of matching receptions represented by this row. Unique stations groups repeats by sender. Each All messages row represents one reception, so its count is 1.";
 if(name=="Target")return "Station the sender addresses. Blank or -- means no individual target was identified, as with CQ, beacon or free-text messages. The Country column describes the sender, not this target.";
 if(name=="Meaning")return "Parsed message category, such as CQ, Grid, Report, R-report, RR73, Text or Beacon. Text can include free text or uninterpreted data; it is not automatically low confidence or balloon telemetry.";
 if(name=="Message")return "Decoded message text. Unique stations shows the sender's latest matching message; All messages keeps each reception and repeat. A leading ? marks a low-confidence reception shown for inspection.";
 if(name=="Msg first heard")return "First recorded occurrence of this exact message from this sender, across bands and modes, within the session/clear scope or retained history when enabled. Repeats keep the original time. Shows seconds ago through 600 seconds, then local date/time; sorting uses the original timestamp.";
 if(name=="Station first heard")return "First recorded message of any kind from this sender, across bands and modes, within the session/clear scope or retained history when enabled. Later messages do not reset it. Shows seconds ago through 600 seconds, then local date/time; sorting uses the original timestamp.";
 if(name=="Date/time")return "Reception date and time in this computer's time zone, including its UTC offset. Sorting uses the original timestamp. Epochs and radio protocol times remain stored in UTC.";
 if(name=="Source")return "Stored origin label, such as live or imported legacy data. Loading saved history preserves its original source label; it does not mean the station was heard again now. This label is separate from decoder confidence.";
 if(name=="Confidence")return "Relative FT8/FT4 decoder score, not an accuracy percentage or SNR. Low marks a questionable whole message; sender, target and grid may be wrong. -- means no comparable score is available. Low-confidence receptions cannot establish map positions, statistics, bearings or alerts. Unflagged decodes can still be wrong.";
 if(name=="Stations")return "Number of distinct identified senders heard from this country within the selected filters and session/history scope. Repeated messages do not increase the station count. Unresolved sender hashes are excluded.";
 if(name=="Avg. distance")return "Arithmetic mean of the latest matching distance from each distinct sender in this country, measured from your home grid. Missing distances are excluded. [C] means the average includes country-based estimates. Each located sender contributes once.";
 if(name=="Avg. SNR dB")return "Arithmetic mean, in decibels, of each distinct sender's latest matching receiver SNR. Each sender contributes once. This is not combined signal power or an average decoder-confidence score.";
 if(name=="Last 5 senders")return "Up to five distinct sender callsigns most recently heard from this country within the selected filters and session/history scope. Newest is listed first; repeated messages do not create duplicate names.";
 if(name=="Last heard")return "Most recent matching reception from this country. Shows seconds ago through 600 seconds, then local date/time. Sorting uses the underlying reception timestamp.";
 if(name=="Mode / band")return "Digital mode and amateur radio band for this individual reception. The band is derived from its dial frequency; the mode identifies the decoding protocol.";
 return nullptr;
}
// Keep native headers for sorting, reordering, resizing and their context menu.
// Add the explanation only over the clipped header row, never over data cells.
inline void tableHeadersWithHelp(const char* distanceUnit,bool historyAll){
 ImGui::TableHeadersRow();auto* table=GImGui->CurrentTable;const int column=ImGui::TableGetHoveredColumn();
 if(column<0||column>=ImGui::TableGetColumnCount()||!ImGui::IsWindowHovered()||ImGui::IsAnyItemActive())return;
 // Native headers leave the last column's clip rectangle active. Use the
 // table-wide clipped bounds so explanations also work on the other columns.
 if(!ImGui::IsMouseHoveringRect({table->InnerClipRect.Min.x,std::max(table->RowPosY1,table->InnerClipRect.Min.y)},{table->InnerClipRect.Max.x,std::min(table->RowPosY2,table->InnerClipRect.Max.y)},false))return;
 const auto* name=ImGui::TableGetColumnName(column);const auto* help=tableColumnHelp(name);if(!help)return;
 ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePreviousTooltip,ImGuiWindowFlags_None);ImGui::PushTextWrapPos(ImGui::GetFontSize()*34);
 ImGui::TextUnformatted(name);ImGui::Separator();ImGui::TextUnformatted(help);
 const std::string_view key=name;
 if(key=="Target"||key=="Sender"){ImGui::Spacing();ImGui::TextUnformatted(unresolvedCallHelp());}
 if(key=="Distance"||key=="Avg. distance")ImGui::Text("Distances use your selected distance unit: %s.",distanceUnit);
 if(key=="Msg first heard"||key=="Station first heard")ImGui::TextUnformatted(historyAll?"First-heard scope: retained history after any view clear.":"First-heard scope: current session after any view clear.");
 ImGui::Spacing();if(table->Flags&ImGuiTableFlags_Sortable)ImGui::TextDisabled("Click to sort; click again to reverse.");
 if(table->Flags&ImGuiTableFlags_Reorderable)ImGui::TextDisabled("Drag a header to reorder columns.");
 if(table->Flags&ImGuiTableFlags_Resizable)ImGui::TextDisabled("Drag column borders to resize.");
 if(table->Flags&ImGuiTableFlags_Hideable)ImGui::TextDisabled("Right-click a header to choose visible columns.");
 ImGui::PopTextWrapPos();ImGui::EndTooltip();
}
}
