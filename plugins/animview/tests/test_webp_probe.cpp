// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Headless tests for the pure WebP probe + libwebp linkage.
// This is the support-grade seam: a user's "this .webp won't animate" becomes a
// byte fixture here, reproduced without Opus or a GPU.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "animview/webp_probe.h"
#include "animview/webp_version.h"

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what);
    ++g_failures;
  }
}

void PutFourCC(std::vector<uint8_t>& b, const char* tag) {
  b.insert(b.end(), tag, tag + 4);
}

void PutU32LE(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(v & 0xff);
  b.push_back((v >> 8) & 0xff);
  b.push_back((v >> 16) & 0xff);
  b.push_back((v >> 24) & 0xff);
}

void PutU24LE(std::vector<uint8_t>& b, uint32_t v) {
  b.push_back(v & 0xff);
  b.push_back((v >> 8) & 0xff);
  b.push_back((v >> 16) & 0xff);
}

// Build a minimal RIFF/WEBP whose first chunk is VP8X with the given flags byte
// and canvas dimensions.
std::vector<uint8_t> MakeVP8X(uint8_t flags, int width, int height) {
  std::vector<uint8_t> b;
  PutFourCC(b, "RIFF");
  PutU32LE(b, 0);  // file size (ignored by probe)
  PutFourCC(b, "WEBP");
  PutFourCC(b, "VP8X");
  PutU32LE(b, 10);          // VP8X payload size
  b.push_back(flags);       // flags @ offset 20
  b.push_back(0);           // reserved
  b.push_back(0);
  b.push_back(0);
  PutU24LE(b, static_cast<uint32_t>(width - 1));   // canvas width-1
  PutU24LE(b, static_cast<uint32_t>(height - 1));  // canvas height-1
  return b;
}

// A minimal still WebP (lossy): first chunk "VP8 ".
std::vector<uint8_t> MakeStillVP8() {
  std::vector<uint8_t> b;
  PutFourCC(b, "RIFF");
  PutU32LE(b, 0);
  PutFourCC(b, "WEBP");
  PutFourCC(b, "VP8 ");
  PutU32LE(b, 0);
  return b;
}

}  // namespace

int main() {
  using namespace animview;

  // Animated: VP8X with the animation flag (0x02).
  {
    auto f = MakeVP8X(0x02, 100, 200);
    Check(IsAnimatedWebp(f.data(), f.size()), "animated VP8X detected");
    Size s = WebpCanvasSize(f.data(), f.size());
    Check(s.cx == 100 && s.cy == 200, "canvas size 100x200");
  }

  // Extended but NOT animated (e.g. alpha flag only, 0x10).
  {
    auto f = MakeVP8X(0x10, 64, 64);
    Check(!IsAnimatedWebp(f.data(), f.size()), "non-animated VP8X rejected");
  }

  // Still lossy WebP: not animated.
  {
    auto f = MakeStillVP8();
    Check(!IsAnimatedWebp(f.data(), f.size()), "still VP8 rejected");
  }

  // Not WebP at all.
  {
    const uint8_t junk[] = {'G', 'I', 'F', '8', '9', 'a', 0, 0, 0, 0,
                            0,   0,   0,   0,   0,   0,   0, 0, 0, 0, 0};
    Check(!IsAnimatedWebp(junk, sizeof(junk)), "non-WebP rejected");
  }

  // Too short to decide.
  {
    const uint8_t tiny[] = {'R', 'I', 'F', 'F'};
    Check(!IsAnimatedWebp(tiny, sizeof(tiny)), "short buffer rejected");
    Check(!IsAnimatedWebp(nullptr, 0), "null buffer rejected");
  }

  // libwebp is fetched, built and linked (build-spine check).
  {
    std::string v = WebpLibVersion();
    Check(!v.empty() && v != "0.0.0", "libwebp linked (version non-empty)");
    std::printf("libwebp version: %s\n", v.c_str());
  }

  if (g_failures == 0) {
    std::printf("All animview_core tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", g_failures);
  return 1;
}
