// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/gif_player.h"

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace animview {
namespace {

// GIF frame disposal methods (from /grctlext/Disposal).
enum Disposal { kDispNone = 0, kDispDoNotDispose = 1, kDispRestoreBg = 2, kDispRestorePrev = 3 };

constexpr int kDefaultDelayMs = 100;  // browsers clamp very small/zero delays

UINT ReadMetaUInt(IWICMetadataQueryReader* r, const wchar_t* name, UINT fallback) {
  if (r == nullptr) return fallback;
  PROPVARIANT v;
  PropVariantInit(&v);
  UINT out = fallback;
  if (SUCCEEDED(r->GetMetadataByName(name, &v))) {
    if (v.vt == VT_UI2) out = v.uiVal;
    else if (v.vt == VT_UI4) out = v.ulVal;
    else if (v.vt == VT_UI1) out = v.bVal;
  }
  PropVariantClear(&v);
  return out;
}

}  // namespace

struct GifPlayer::Impl {
  std::vector<uint8_t> data;  // kept alive for the WIC memory stream
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;

  int canvas_w = 0;
  int canvas_h = 0;
  UINT frame_count = 0;
  uint32_t loop_count = 0;
  bool valid = false;

  // Running composition state.
  std::vector<uint8_t> canvas;        // BGRA, canvas_w*canvas_h*4
  std::vector<uint8_t> saved_canvas;  // for kDispRestorePrev
  UINT next_index = 0;
  int prev_disposal = kDispNone;
  RECT prev_rect{0, 0, 0, 0};

  void ClearCanvas() { std::fill(canvas.begin(), canvas.end(), uint8_t{0}); }
};

GifPlayer::GifPlayer() : p_(std::make_unique<Impl>()) {}
GifPlayer::~GifPlayer() = default;

bool GifPlayer::Open(const uint8_t* data, size_t len) {
  if (data == nullptr || len < 6) return false;
  if (std::memcmp(data, "GIF87a", 6) != 0 && std::memcmp(data, "GIF89a", 6) != 0) {
    return false;
  }

  p_->data.assign(data, data + len);

  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&p_->factory)))) {
    return false;
  }

  ComPtr<IWICStream> stream;
  if (FAILED(p_->factory->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromMemory(p_->data.data(),
                                          static_cast<DWORD>(p_->data.size())))) {
    return false;
  }

  if (FAILED(p_->factory->CreateDecoderFromStream(
          stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &p_->decoder))) {
    return false;
  }

  if (FAILED(p_->decoder->GetFrameCount(&p_->frame_count)) || p_->frame_count == 0) {
    return false;
  }

  // Canvas size from the logical screen descriptor; fall back to first frame.
  ComPtr<IWICMetadataQueryReader> global;
  if (SUCCEEDED(p_->decoder->GetMetadataQueryReader(&global))) {
    p_->canvas_w = static_cast<int>(ReadMetaUInt(global.Get(), L"/logscrdesc/Width", 0));
    p_->canvas_h = static_cast<int>(ReadMetaUInt(global.Get(), L"/logscrdesc/Height", 0));
  }
  if (p_->canvas_w <= 0 || p_->canvas_h <= 0) {
    ComPtr<IWICBitmapFrameDecode> f0;
    if (FAILED(p_->decoder->GetFrame(0, &f0))) return false;
    UINT fw = 0, fh = 0;
    f0->GetSize(&fw, &fh);
    p_->canvas_w = static_cast<int>(fw);
    p_->canvas_h = static_cast<int>(fh);
  }
  if (p_->canvas_w <= 0 || p_->canvas_h <= 0) return false;

  p_->canvas.assign(static_cast<size_t>(p_->canvas_w) * p_->canvas_h * 4, 0);
  p_->next_index = 0;
  p_->prev_disposal = kDispNone;
  p_->prev_rect = RECT{0, 0, 0, 0};
  p_->valid = true;
  return true;
}

bool GifPlayer::Valid() const { return p_->valid; }
int GifPlayer::CanvasWidth() const { return p_->canvas_w; }
int GifPlayer::CanvasHeight() const { return p_->canvas_h; }
size_t GifPlayer::FrameCount() const { return p_->frame_count; }
uint32_t GifPlayer::LoopCount() const { return p_->loop_count; }

void GifPlayer::Rewind() {
  p_->ClearCanvas();
  p_->next_index = 0;
  p_->prev_disposal = kDispNone;
  p_->prev_rect = RECT{0, 0, 0, 0};
}

bool GifPlayer::NextFrame(Frame& out) {
  if (!p_->valid) return false;
  if (p_->next_index >= p_->frame_count) Rewind();  // loop

  const UINT index = p_->next_index;

  // 1) Apply the PREVIOUS frame's disposal before drawing this one.
  if (p_->prev_disposal == kDispRestoreBg) {
    const RECT& r = p_->prev_rect;
    const int rl = (std::max)(0, static_cast<int>(r.left));
    for (int y = (std::max)(0, static_cast<int>(r.top));
         y < r.bottom && y < p_->canvas_h; ++y) {
      uint8_t* row = p_->canvas.data() + (static_cast<size_t>(y) * p_->canvas_w + rl) * 4;
      const int span = (std::min)(static_cast<int>(r.right), p_->canvas_w) - rl;
      if (span > 0) std::memset(row, 0, static_cast<size_t>(span) * 4);
    }
  } else if (p_->prev_disposal == kDispRestorePrev && !p_->saved_canvas.empty()) {
    p_->canvas = p_->saved_canvas;
  }

  // 2) Decode this frame to 32bpp BGRA.
  ComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(p_->decoder->GetFrame(index, &frame))) return false;

  ComPtr<IWICFormatConverter> conv;
  if (FAILED(p_->factory->CreateFormatConverter(&conv)) ||
      FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom))) {
    return false;
  }

  UINT fw = 0, fh = 0;
  frame->GetSize(&fw, &fh);

  // Frame placement + timing + disposal from metadata.
  ComPtr<IWICMetadataQueryReader> meta;
  int left = 0, top = 0, delay_cs = 0, disposal = kDispNone;
  if (SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
    left = static_cast<int>(ReadMetaUInt(meta.Get(), L"/imgdesc/Left", 0));
    top = static_cast<int>(ReadMetaUInt(meta.Get(), L"/imgdesc/Top", 0));
    delay_cs = static_cast<int>(ReadMetaUInt(meta.Get(), L"/grctlext/Delay", 0));
    disposal = static_cast<int>(ReadMetaUInt(meta.Get(), L"/grctlext/Disposal", 0));
  }

  RECT rect{left, top, left + static_cast<int>(fw), top + static_cast<int>(fh)};

  // Save canvas before drawing if this frame wants "restore to previous".
  if (disposal == kDispRestorePrev) {
    p_->saved_canvas = p_->canvas;
  }

  // 3) Copy frame pixels, honouring transparency (alpha 0 = leave canvas).
  std::vector<uint8_t> fb(static_cast<size_t>(fw) * fh * 4);
  const UINT stride = fw * 4;
  if (FAILED(conv->CopyPixels(nullptr, stride, static_cast<UINT>(fb.size()), fb.data()))) {
    return false;
  }
  for (int y = 0; y < static_cast<int>(fh); ++y) {
    const int cy = top + y;
    if (cy < 0 || cy >= p_->canvas_h) continue;
    const uint8_t* src = fb.data() + static_cast<size_t>(y) * stride;
    uint8_t* dst = p_->canvas.data() + (static_cast<size_t>(cy) * p_->canvas_w + left) * 4;
    for (int x = 0; x < static_cast<int>(fw); ++x) {
      const int cx = left + x;
      if (cx < 0 || cx >= p_->canvas_w) continue;
      const uint8_t* s = src + static_cast<size_t>(x) * 4;
      if (s[3] != 0) {  // opaque pixel overwrites; transparent leaves canvas
        uint8_t* d = dst + static_cast<size_t>(x) * 4;
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = 255;
      }
    }
  }

  // 4) Emit the composited canvas.
  out.width = p_->canvas_w;
  out.height = p_->canvas_h;
  out.duration_ms = (delay_cs > 0) ? delay_cs * 10 : kDefaultDelayMs;
  out.bgra = p_->canvas;

  p_->prev_disposal = disposal;
  p_->prev_rect = rect;
  p_->next_index = index + 1;
  return true;
}

}  // namespace animview
