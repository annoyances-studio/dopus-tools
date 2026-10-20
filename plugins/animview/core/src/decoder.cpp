// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/decoder.h"

#include "animview/anim_source.h"

namespace animview {

bool DecodeFirstFrame(const uint8_t* data, size_t len, Frame& out) {
  // Format-agnostic: the factory picks WebP or GIF; the first NextFrame yields
  // the first composited frame. Used for thumbnails / the still path.
  auto src = OpenAnimSource(data, len);
  if (!src) return false;
  return src->NextFrame(out);
}

}  // namespace animview
