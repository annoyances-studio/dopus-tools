// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Headless test for the streaming AnimPlayer: frame stepping, per-frame
// duration, and automatic looping. Fixture synthesized in memory.

#include <array>
#include <cstdio>
#include <vector>

#include "animview/anim_player.h"
#include "webp_test_util.h"

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what);
    ++g_failures;
  }
}

// Expect the frame's top-left pixel to equal an RGBA colour in BGRA order.
bool PixelIs(const animview::Frame& f, uint8_t r, uint8_t g, uint8_t b) {
  return f.bgra.size() >= 4 && f.bgra[0] == b && f.bgra[1] == g && f.bgra[2] == r;
}

}  // namespace

int main() {
  const int W = 8, H = 8;
  const std::vector<std::array<uint8_t, 4>> colors = {
      {10, 20, 30, 255}, {40, 50, 60, 255}, {70, 80, 90, 255}};
  const auto bytes = avtest::BuildAnimation(W, H, colors, 50);
  Check(!bytes.empty(), "animation synthesized");

  animview::AnimPlayer p;
  Check(p.Open(bytes.data(), bytes.size()), "player opened");
  Check(p.CanvasWidth() == W && p.CanvasHeight() == H, "canvas 8x8");
  Check(p.FrameCount() == 3, "frame count 3");

  animview::Frame f;

  Check(p.NextFrame(f) && PixelIs(f, 10, 20, 30), "frame 0 colour");
  Check(f.duration_ms == 50, "frame 0 duration 50ms");

  Check(p.NextFrame(f) && PixelIs(f, 40, 50, 60), "frame 1 colour");
  Check(f.duration_ms == 50, "frame 1 duration 50ms");

  Check(p.NextFrame(f) && PixelIs(f, 70, 80, 90), "frame 2 colour");

  // Past the last frame it must loop back to frame 0.
  Check(p.NextFrame(f) && PixelIs(f, 10, 20, 30), "loops back to frame 0");
  Check(f.duration_ms == 50, "looped frame duration 50ms");

  // SeekToFrame (used by "previous frame" stepping).
  Check(p.SeekToFrame(2, f) && PixelIs(f, 70, 80, 90), "seek to frame 2");
  Check(p.SeekToFrame(0, f) && PixelIs(f, 10, 20, 30), "seek back to frame 0");

  if (g_failures == 0) {
    std::printf("All animview_anim tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", g_failures);
  return 1;
}
