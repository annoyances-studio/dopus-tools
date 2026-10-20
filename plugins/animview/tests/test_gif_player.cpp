// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Headless test for the WIC-based GifPlayer: frame decode, colours, per-frame
// delay, and looping. GIF fixture synthesized in memory via WIC.

#include <windows.h>

#include <array>
#include <cstdio>
#include <vector>

#include "animview/gif_player.h"
#include "gif_test_util.h"

int main() {
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
    std::printf("FAIL: CoInitializeEx\n");
    return 1;
  }

  int failures = 0;
  auto Check = [&](bool cond, const char* what) {
    if (!cond) {
      std::printf("FAIL: %s\n", what);
      ++failures;
    }
  };

  const int W = 4, H = 4;
  const std::vector<std::array<uint8_t, 3>> colors = {{220, 20, 20}, {20, 200, 20}};  // red, green
  const auto bytes = avtest::BuildGif(W, H, colors, 5);  // 5 cs = 50 ms
  Check(!bytes.empty(), "gif synthesized");

  if (!bytes.empty()) {
    animview::GifPlayer p;
    Check(p.Open(bytes.data(), bytes.size()), "gif opened");
    Check(p.CanvasWidth() == W && p.CanvasHeight() == H, "canvas 4x4");
    Check(p.FrameCount() == 2, "frame count 2");

    animview::Frame f;
    // Frame 0 — red: BGRA => B low, G low, R high.
    Check(p.NextFrame(f) && f.bgra.size() >= 4 && f.bgra[2] > 180 && f.bgra[1] < 80 &&
              f.bgra[0] < 80,
          "frame 0 is red");
    Check(f.duration_ms == 50, "frame 0 duration 50ms");

    // Frame 1 — green.
    Check(p.NextFrame(f) && f.bgra.size() >= 4 && f.bgra[1] > 150 && f.bgra[2] < 80 &&
              f.bgra[0] < 80,
          "frame 1 is green");

    // Loops back to red.
    Check(p.NextFrame(f) && f.bgra.size() >= 4 && f.bgra[2] > 180 && f.bgra[1] < 80,
          "loops back to frame 0 (red)");
  }

  CoUninitialize();

  if (failures == 0) {
    std::printf("All animview_gif tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", failures);
  return 1;
}
