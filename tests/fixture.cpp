// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "ft8_etc/gen_ft8.h"
#include "ft8_etc/gen_ft4.h"
#include <fstream>
#include <vector>
int main(int argc,char** argv){if(argc!=3&&argc!=4)return 2;mshv_init();std::vector<short> wave(864000,0);bool ft4=std::string(argv[1])=="FT4";int count;const char* message=argc==4?argv[3]:"CQ K1ABC FN42";
 // Upstream generators use fixed 48 kHz symbol lengths regardless of their rate argument.
 if(ft4){GenFt4 gen(false);count=gen.genft4(message,wave.data(),48000,1000);}else{GenFt8 gen(false);count=gen.genft8(message,wave.data(),48000,1000);}
 std::vector<short> slot(ft4?90000:180000,0);int start=ft4?0:6000;for(size_t i=0;i<size_t(count)/4&&i+start<slot.size();i++)slot[i+start]=wave[4*i];
 std::ofstream file(argv[2],std::ios::binary);file.write(reinterpret_cast<const char*>(slot.data()),slot.size()*2);return file?0:1;
}
