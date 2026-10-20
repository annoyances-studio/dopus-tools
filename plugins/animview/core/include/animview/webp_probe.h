// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// Pure WebP container inspection — no libwebp decode, no Windows, no Opus.
// Used by the Opus shell's DVP_IdentifyFileBytes to decide whether a file is an
// ANIMATED WebP (which we handle) versus a still WebP (which Opus already shows).
//
// WebP is a RIFF container:
//   bytes  0..3  "RIFF"
//   bytes  4..7  file size (LE, ignored here)
//   bytes  8..11 "WEBP"
//   bytes 12..15 first chunk FourCC
// A still image's first chunk is "VP8 " (lossy) or "VP8L" (lossless).
// An extended file's first chunk is "VP8X", whose 10-byte payload begins with a
// flags byte; the Animation flag is bit 1 (0x02), and an animated file then
// carries "ANIM"/"ANMF" chunks. The VP8X payload also holds canvas width-1 and
// height-1 as 24-bit LE values.
//
#include <cstddef>
#include <cstdint>

namespace animview {

struct Size {
  int cx = 0;
  int cy = 0;
};

// True only for an ANIMATED WebP. False for still WebP, non-WebP, or a buffer
// too short to decide. Safe to call on a short file-header slice.
bool IsAnimatedWebp(const uint8_t* data, size_t len) noexcept;

// Canvas size from a VP8X header, or {0,0} if not determinable from the slice.
Size WebpCanvasSize(const uint8_t* data, size_t len) noexcept;

}  // namespace animview
