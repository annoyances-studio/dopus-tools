// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// Streaming animated-WebP player — pure (no Windows/Opus/GDI), libwebp hidden in
// the .cpp via pimpl. Keeps only the decoder + current frame alive (O(1 canvas)
// memory), decoding each frame on demand and looping. This is the structural
// answer to the fork base's "all frames resident" OOM.
//
#include <cstddef>
#include <cstdint>
#include <memory>

#include "animview/anim_source.h"
#include "animview/decoder.h"  // Frame

namespace animview {

// WebP animation source (libwebp WebPAnimDecoder).
class AnimPlayer : public AnimSource {
 public:
  AnimPlayer();
  ~AnimPlayer() override;
  AnimPlayer(const AnimPlayer&) = delete;
  AnimPlayer& operator=(const AnimPlayer&) = delete;

  // Copies the bytes (kept alive for the decoder's lifetime). Returns false if
  // the data is not a decodable WebP.
  bool Open(const uint8_t* data, size_t len);

  bool Valid() const override;
  int CanvasWidth() const override;
  int CanvasHeight() const override;
  size_t FrameCount() const override;
  uint32_t LoopCount() const override;
  bool NextFrame(Frame& out) override;
  void Rewind() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> p_;
};

}  // namespace animview
