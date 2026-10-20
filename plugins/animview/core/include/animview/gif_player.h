// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// GIF animation source via WIC (built into Windows — no third-party dependency).
// WIC hands us raw frames + GIF metadata; we composite with disposal ourselves,
// keeping only the running canvas (and one saved copy for "restore to previous").
//
#include <cstddef>
#include <cstdint>
#include <memory>

#include "animview/anim_source.h"
#include "animview/decoder.h"  // Frame

namespace animview {

class GifPlayer : public AnimSource {
 public:
  GifPlayer();
  ~GifPlayer() override;
  GifPlayer(const GifPlayer&) = delete;
  GifPlayer& operator=(const GifPlayer&) = delete;

  // Copies the bytes (kept alive for the WIC stream). COM must be initialised on
  // this thread. Returns false if not a decodable GIF.
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
