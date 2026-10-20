// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
// Test-only helper: synthesize a multi-frame GIF in memory via WIC's encoder.
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <vector>

namespace avtest {

using Microsoft::WRL::ComPtr;

// Build a GIF of full-canvas solid-colour frames (RGB), each shown delayCs
// centiseconds. Returns the encoded bytes, or empty on failure.
inline std::vector<uint8_t> BuildGif(int w, int h,
                                     const std::vector<std::array<uint8_t, 3>>& colors,
                                     int delayCs) {
  std::vector<uint8_t> result;

  ComPtr<IWICImagingFactory> factory;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory)))) {
    return result;
  }
  ComPtr<IStream> stream;
  if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) return result;

  ComPtr<IWICBitmapEncoder> enc;
  if (FAILED(factory->CreateEncoder(GUID_ContainerFormatGif, nullptr, &enc))) return result;
  if (FAILED(enc->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return result;

  for (const auto& c : colors) {
    std::vector<uint8_t> buf(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < buf.size(); i += 4) {
      buf[i + 0] = c[2];  // B
      buf[i + 1] = c[1];  // G
      buf[i + 2] = c[0];  // R
      buf[i + 3] = 255;   // A
    }
    ComPtr<IWICBitmap> bmp;
    if (FAILED(factory->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA, w * 4,
                                               static_cast<UINT>(buf.size()), buf.data(),
                                               &bmp))) {
      return result;
    }
    ComPtr<IWICBitmapFrameEncode> fe;
    ComPtr<IPropertyBag2> props;
    if (FAILED(enc->CreateNewFrame(&fe, &props)) || FAILED(fe->Initialize(props.Get()))) {
      return result;
    }
    fe->SetSize(w, h);

    ComPtr<IWICMetadataQueryWriter> mw;
    if (SUCCEEDED(fe->GetMetadataQueryWriter(&mw))) {
      PROPVARIANT v;
      PropVariantInit(&v);
      v.vt = VT_UI2;
      v.uiVal = static_cast<USHORT>(delayCs);
      mw->SetMetadataByName(L"/grctlext/Delay", &v);
      PropVariantClear(&v);
    }

    if (FAILED(fe->WriteSource(bmp.Get(), nullptr)) || FAILED(fe->Commit())) return result;
  }

  if (FAILED(enc->Commit())) return result;

  HGLOBAL hg = nullptr;
  if (FAILED(GetHGlobalFromStream(stream.Get(), &hg)) || hg == nullptr) return result;
  const SIZE_T sz = GlobalSize(hg);
  void* p = GlobalLock(hg);
  if (p != nullptr && sz > 0) {
    result.assign(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + sz);
  }
  GlobalUnlock(hg);
  return result;
}

}  // namespace avtest
