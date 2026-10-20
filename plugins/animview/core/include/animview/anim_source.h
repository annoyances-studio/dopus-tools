// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// Format-agnostic animation source: the viewer drives this, not a specific
// decoder. WebP (libwebp) and GIF (WIC) each implement it; a factory picks by
// header bytes. Every implementation keeps only the current composited canvas
// (O(1) memory) and loops.
//
#include <cstddef>
#include <cstdint>
#include <memory>

#include "animview/decoder.h"  // Frame

namespace animview {

class AnimSource {
 public:
  virtual ~AnimSource() = default;

  virtual bool Valid() const = 0;
  virtual int CanvasWidth() const = 0;
  virtual int CanvasHeight() const = 0;
  virtual size_t FrameCount() const = 0;
  virtual uint32_t LoopCount() const = 0;  // 0 == loop forever

  // Fill `out` with the next composited frame (BGRA, top-down) + its duration.
  // Loops automatically. Returns false only if the source is not open.
  virtual bool NextFrame(Frame& out) = 0;
  virtual void Rewind() = 0;
};

// Detect WebP vs GIF from the header and return an opened source, or nullptr if
// the bytes are neither / not decodable. COM must be initialised on the calling
// thread for GIF (Opus does this; tests call CoInitializeEx).
std::unique_ptr<AnimSource> OpenAnimSource(const uint8_t* data, size_t len);

}  // namespace animview
