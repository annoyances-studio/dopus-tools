// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/webp_probe.h"

#include <cstring>

namespace animview {
namespace {

inline bool FourCC(const uint8_t* p, const char (&tag)[5]) noexcept {
  return std::memcmp(p, tag, 4) == 0;
}

// Read a 24-bit little-endian value.
inline int U24LE(const uint8_t* p) noexcept {
  return static_cast<int>(p[0]) | (static_cast<int>(p[1]) << 8) |
         (static_cast<int>(p[2]) << 16);
}

// Returns true and leaves VP8X payload reachable if this is a RIFF/WEBP whose
// first chunk is VP8X and the slice is long enough to read the flags byte.
bool HasVP8X(const uint8_t* data, size_t len) noexcept {
  // Need through the VP8X flags byte at offset 20.
  if (data == nullptr || len < 21) return false;
  if (!FourCC(data + 0, "RIFF")) return false;
  if (!FourCC(data + 8, "WEBP")) return false;
  if (!FourCC(data + 12, "VP8X")) return false;
  return true;
}

}  // namespace

bool IsAnimatedWebp(const uint8_t* data, size_t len) noexcept {
  if (!HasVP8X(data, len)) return false;
  // VP8X: FourCC(12..15) + size(16..19) + payload; payload[0] = flags @ 20.
  // Animation flag = bit 1 (0x02).
  const uint8_t flags = data[20];
  return (flags & 0x02) != 0;
}

Size WebpCanvasSize(const uint8_t* data, size_t len) noexcept {
  // VP8X canvas: width-1 @24..26, height-1 @27..29 (24-bit LE each).
  if (!HasVP8X(data, len) || len < 30) return Size{};
  Size s;
  s.cx = U24LE(data + 24) + 1;
  s.cy = U24LE(data + 27) + 1;
  return s;
}

}  // namespace animview
