// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "table_help.h"
#include <iostream>
#include <string>
int main(){
 for(auto* name:{"Sender","Country","Grid","SNR dB","Distance","Msg age","Mod","Band","Dial MHz","Audio Hz","DT s","Bearing","Count","Target","Meaning","Message","Msg first heard","Station first heard","Date/time","Source","Confidence","Stations","Avg. distance","Avg. SNR dB","Last 5 senders","Last heard","Mode / band"})if(!dx::tableColumnHelp(name)){std::cerr<<name<<" lacks an explanation\n";return 1;}
 if(!dx::containsUnresolvedCall("<2:2859538>")||!dx::containsUnresolvedCall("<1:123> DK7UY JO44")||!dx::containsUnresolvedCall("<...>")||dx::containsUnresolvedCall("<DK7UY>")||dx::containsUnresolvedCall("DK7UY"))return 2;
 const std::string help=dx::unresolvedCallHelp();for(auto text:{"12 minutes","10-, 12- and 22-bit","exactly one","outside that window","multiple calls","later hashed receptions","not by itself"})if(help.find(text)==std::string::npos)return 3;
 std::cout<<"All column descriptions, unresolved-hash detection and cautious resolution explanation passed\n";
}
