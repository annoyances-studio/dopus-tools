// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

namespace animview {

// Read a whole file into memory. Empty on failure. Capped so a pathological
// file can't balloon memory (WebP stills/animations are comfortably under this).
std::vector<uint8_t> ReadWholeFile(LPCWSTR path);

}  // namespace animview
