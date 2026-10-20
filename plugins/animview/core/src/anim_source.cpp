// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/anim_source.h"

#include <cstring>

#include "animview/anim_player.h"
#include "animview/gif_player.h"

namespace animview {

std::unique_ptr<AnimSource> OpenAnimSource(const uint8_t* data, size_t len) {
  if (data == nullptr || len < 12) return nullptr;

  // GIF
  if (len >= 6 &&
      (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0)) {
    auto gif = std::make_unique<GifPlayer>();
    if (gif->Open(data, len)) return gif;
    return nullptr;
  }

  // WebP (RIFF....WEBP)
  if (std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0) {
    auto webp = std::make_unique<AnimPlayer>();
    if (webp->Open(data, len)) return webp;
    return nullptr;
  }

  return nullptr;
}

}  // namespace animview
