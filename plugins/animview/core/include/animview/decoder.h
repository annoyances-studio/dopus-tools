// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// Pure WebP frame decode — no Windows, no Opus, no GDI. Depends on libwebp.
// Increment #2 exposes only the first composited frame (for thumbnails + the
// still preview). The full frame iterator for animation arrives in increment #3.
//
#include <cstddef>
#include <cstdint>
#include <vector>

namespace animview {

struct Frame {
  int width = 0;
  int height = 0;
  // Pixels in **BGRA** byte order, top-down (row 0 first) — ready to memcpy into
  // a 32-bit top-down Windows DIBSection. Size == width * height * 4.
  std::vector<uint8_t> bgra;
  int duration_ms = 0;  // display duration of this frame
};

// Decode the first (fully composited) frame of a WebP — animated or still.
// Returns false if the data is not a decodable WebP. No libwebp types leak out.
bool DecodeFirstFrame(const uint8_t* data, size_t len, Frame& out);

}  // namespace animview
