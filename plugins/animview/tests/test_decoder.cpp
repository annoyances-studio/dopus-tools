// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Decode test: synthesize a 2-frame animated WebP in memory (WebPAnimEncoder),
// then decode its first frame through the pure core and check it. No fixture
// file, no network, no Opus, no GPU.

#include <cstdint>
#include <cstdio>
#include <vector>

#include <webp/encode.h>
#include <webp/mux.h>

#include "animview/decoder.h"
#include "animview/webp_probe.h"

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
  if (!cond) {
    std::printf("FAIL: %s\n", what);
    ++g_failures;
  }
}

std::vector<uint8_t> Solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  std::vector<uint8_t> v(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < v.size(); i += 4) {
    v[i + 0] = r;
    v[i + 1] = g;
    v[i + 2] = b;
    v[i + 3] = a;
  }
  return v;
}

bool AddFrame(WebPAnimEncoder* enc, int w, int h, const std::vector<uint8_t>& rgba, int ts) {
  WebPConfig cfg;
  if (!WebPConfigInit(&cfg)) return false;
  cfg.lossless = 1;
  cfg.quality = 100;

  WebPPicture pic;
  if (!WebPPictureInit(&pic)) return false;
  pic.use_argb = 1;
  pic.width = w;
  pic.height = h;

  bool ok = false;
  if (WebPPictureImportRGBA(&pic, rgba.data(), w * 4)) {
    ok = WebPAnimEncoderAdd(enc, &pic, ts, &cfg) != 0;
  }
  WebPPictureFree(&pic);
  return ok;
}

}  // namespace

int main() {
  const int W = 4, H = 4;
  const auto f0 = Solid(W, H, 10, 20, 30, 255);     // RGBA
  const auto f1 = Solid(W, H, 200, 150, 100, 255);

  WebPAnimEncoderOptions eo;
  WebPAnimEncoderOptionsInit(&eo);
  WebPAnimEncoder* enc = WebPAnimEncoderNew(W, H, &eo);
  Check(enc != nullptr, "encoder created");

  bool added = enc && AddFrame(enc, W, H, f0, 0) && AddFrame(enc, W, H, f1, 100);
  Check(added, "frames added");
  if (enc) WebPAnimEncoderAdd(enc, nullptr, 200, nullptr);  // finalize end time

  WebPData out;
  WebPDataInit(&out);
  const bool assembled = enc && WebPAnimEncoderAssemble(enc, &out) != 0;
  Check(assembled, "assembled animated webp");
  if (enc) WebPAnimEncoderDelete(enc);

  if (assembled) {
    Check(animview::IsAnimatedWebp(out.bytes, out.size), "probe reports animated");

    animview::Frame fr;
    const bool decoded = animview::DecodeFirstFrame(out.bytes, out.size, fr);
    Check(decoded, "decoded first frame");
    if (decoded) {
      Check(fr.width == W && fr.height == H, "decoded dimensions 4x4");
      Check(fr.bgra.size() == static_cast<size_t>(W) * H * 4, "bgra buffer size");
      // frame0 RGBA(10,20,30,255) -> BGRA(30,20,10,255)
      if (fr.bgra.size() >= 4) {
        Check(fr.bgra[0] == 30 && fr.bgra[1] == 20 && fr.bgra[2] == 10 &&
                  fr.bgra[3] == 255,
              "first pixel is frame 0 in BGRA order");
      }
    }
  }
  WebPDataClear(&out);

  if (g_failures == 0) {
    std::printf("All animview_decode tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", g_failures);
  return 1;
}
