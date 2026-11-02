// Copyright 2026 ReLLL. SPDX-License-Identifier: GPL-3.0-or-later
#include "share_card.h"
#include "share_background_bytes.h"
namespace dx {
ShareBackground shareBackground(){return {backgroundBytes,sizeof(backgroundBytes)};}
}
