// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
// Test-only helpers: synthesize WebP in memory via libwebp's encoder + mux.
#include <array>
#include <cstdint>
#include <vector>

#include <webp/encode.h>
#include <webp/mux.h>

namespace avtest {

inline std::vector<uint8_t> Solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  std::vector<uint8_t> v(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < v.size(); i += 4) {
    v[i + 0] = r;
    v[i + 1] = g;
    v[i + 2] = b;
    v[i + 3] = a;
  }
  return v;
}

// Build an animated WebP of solid-colour frames (RGBA), each shown durationMs.
inline std::vector<uint8_t> BuildAnimation(int w, int h,
                                           const std::vector<std::array<uint8_t, 4>>& colors,
                                           int durationMs) {
  std::vector<uint8_t> result;
  WebPAnimEncoderOptions eo;
  if (!WebPAnimEncoderOptionsInit(&eo)) return result;
  WebPAnimEncoder* enc = WebPAnimEncoderNew(w, h, &eo);
  if (enc == nullptr) return result;

  bool ok = true;
  int ts = 0;
  for (const auto& c : colors) {
    const auto rgba = Solid(w, h, c[0], c[1], c[2], c[3]);
    WebPConfig cfg;
    WebPConfigInit(&cfg);
    cfg.lossless = 1;
    cfg.quality = 100;
    WebPPicture pic;
    WebPPictureInit(&pic);
    pic.use_argb = 1;
    pic.width = w;
    pic.height = h;
    if (WebPPictureImportRGBA(&pic, rgba.data(), w * 4)) {
      ok = (WebPAnimEncoderAdd(enc, &pic, ts, &cfg) != 0) && ok;
    } else {
      ok = false;
    }
    WebPPictureFree(&pic);
    ts += durationMs;
  }
  WebPAnimEncoderAdd(enc, nullptr, ts, nullptr);  // finalize

  WebPData out;
  WebPDataInit(&out);
  if (ok && WebPAnimEncoderAssemble(enc, &out)) {
    result.assign(out.bytes, out.bytes + out.size);
  }
  WebPDataClear(&out);
  WebPAnimEncoderDelete(enc);
  return result;
}

}  // namespace avtest
