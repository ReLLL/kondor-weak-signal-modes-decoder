// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "share_card.h"
namespace dx {bool linuxNativeReady();void pollLinuxNative();void linuxOpenUrl(const std::string& url);std::optional<std::string> linuxSavePath(const std::string& title,const std::string& current,const char* pattern);}
namespace dx {void releaseLinuxShareClipboard();}
