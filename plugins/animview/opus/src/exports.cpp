// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview_exports.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "animview/decoder.h"
#include "animview/webp_probe.h"
#include "file_io.h"
#include "viewer.h"

extern HINSTANCE g_hInstance;  // defined in dllmain.cpp

namespace {

// Build a top-down 32-bit BGRA DIBSection from a decoded frame. Opus frees it.
HBITMAP FrameToDIBSection(HWND hWnd, const animview::Frame& f) {
  if (f.width <= 0 || f.height <= 0 ||
      f.bgra.size() != static_cast<size_t>(f.width) * f.height * 4) {
    return nullptr;
  }

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = f.width;
  bi.bmiHeader.biHeight = -f.height;  // negative => top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HDC hdc = GetDC(hWnd);
  HBITMAP hbm = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(hWnd, hdc);

  if (hbm != nullptr && bits != nullptr) {
    std::memcpy(bits, f.bgra.data(), f.bgra.size());
    GdiFlush();
  } else if (hbm != nullptr) {
    DeleteObject(hbm);
    hbm = nullptr;
  }
  return hbm;
}

}  // namespace

// Unique plugin identity. Generated for animview; do not reuse another plugin's.
// {5E8A1C3D-7B42-4F96-9A1E-2C6D0B4F8E71}
static const GUID GUIDPlugin_animview = {
    0x5e8a1c3d, 0x7b42, 0x4f96, {0x9a, 0x1e, 0x2c, 0x6d, 0x0b, 0x4f, 0x8e, 0x71}};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

BOOL DVP_InitEx(LPDVPINITEXDATA /*pInitExData*/) {
  return animview::RegisterViewerClass(g_hInstance) ? TRUE : FALSE;
}

void DVP_Uninit(void) { animview::UnregisterViewerClass(g_hInstance); }

BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA /*pUSBSafeData*/) {
  // No absolute paths or machine-specific state baked in.
  return TRUE;
}

// ---------------------------------------------------------------------------
// Identification
// ---------------------------------------------------------------------------

BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo) {
  if (lpVPInfo->cbSize < VIEWERPLUGININFO_V4_SIZE) {
    return FALSE;  // require the v4 API (Opus 9+).
  }

  // We identify by header bytes, and must override Opus's internal (still) WebP
  // handler for the animated files we claim. We deliberately do NOT claim still
  // WebP (see DVP_IdentifyFileBytes), so Opus keeps showing those itself.
  lpVPInfo->dwFlags = DVPFIF_CanHandleBytes | DVPFIF_OverrideInternal;

  // Version 0.1.0.0  (High = minor,major ; Low = revision,build)
  lpVPInfo->dwVersionHigh = MAKELPARAM(1, 0);
  lpVPInfo->dwVersionLow = MAKELPARAM(0, 0);

  lstrcpynW(lpVPInfo->lpszHandleExts, L".webp;.gif", lpVPInfo->cchHandleExtsMax);
  lstrcpynW(lpVPInfo->lpszName, L"animview", lpVPInfo->cchNameMax);
  lstrcpynW(lpVPInfo->lpszDescription,
            L"Animated WebP and GIF viewer (animview)", lpVPInfo->cchDescriptionMax);
  lstrcpynW(lpVPInfo->lpszCopyright, L"github.com/danmarce", lpVPInfo->cchCopyrightMax);
  lstrcpynW(lpVPInfo->lpszURL, L"https://github.com/danmarce", lpVPInfo->cchURLMax);

  lpVPInfo->dwlMinFileSize = 21;  // need at least through the VP8X flags byte
  lpVPInfo->dwlMaxFileSize = 0;   // no upper limit
  lpVPInfo->dwlMinPreviewFileSize = lpVPInfo->dwlMinFileSize;
  lpVPInfo->dwlMaxPreviewFileSize = lpVPInfo->dwlMaxFileSize;
  lpVPInfo->uiMajorFileType = DVPMajorType_Image;
  lpVPInfo->idPlugin = GUIDPlugin_animview;

  lpVPInfo->hIconSmall = nullptr;
  lpVPInfo->hIconLarge = nullptr;

  return TRUE;
}

namespace {

// Common file-info fields for an image we claim. CanReturnViewer forces Opus to
// use our animated window; CanReturnThumbnail routes thumbnails to DVP_LoadBitmap.
// We never set CanReturnBitmap (that would make Opus show a static image instead).
void FillClaim(LPVIEWERPLUGINFILEINFO fi, int w, int h, const wchar_t* info) {
  fi->dwFlags = DVPFIF_CanReturnViewer | DVPFIF_CanReturnThumbnail;
  fi->wMajorType = DVPMajorType_Image;
  fi->wMinorType = 0;
  fi->iColorSpace = DVPColorSpace_RGB;
  fi->szImageSize.cx = w;
  fi->szImageSize.cy = h;
  fi->iNumBits = 32;
  if (fi->lpszInfo != nullptr) lstrcpynW(fi->lpszInfo, info, fi->cchInfoMax);
}

}  // namespace

BOOL DVP_IdentifyFileBytes(HWND /*hWnd*/, LPTSTR /*lpszName*/, LPBYTE lpData,
                           UINT uiDataSize, LPVIEWERPLUGINFILEINFO lpVPFileInfo,
                           DWORD /*dwStreamFlags*/) {
  const auto* data = reinterpret_cast<const uint8_t*>(lpData);

  // Animated WebP — still WebP stays with Opus's built-in handler.
  if (animview::IsAnimatedWebp(data, uiDataSize)) {
    const animview::Size sz = animview::WebpCanvasSize(data, uiDataSize);
    FillClaim(lpVPFileInfo, sz.cx, sz.cy, L"Animated WebP");
    return TRUE;
  }

  // GIF — claim all (our viewer animates multi-frame and shows single-frame).
  // This is what replaces the old OOM-prone GIF plugin.
  if (uiDataSize >= 10 &&
      (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) {
    const int w = data[6] | (data[7] << 8);   // logical screen width (LE)
    const int h = data[8] | (data[9] << 8);   // logical screen height (LE)
    FillClaim(lpVPFileInfo, w, h, L"GIF");
    return TRUE;
  }

  return FALSE;
}

// ---------------------------------------------------------------------------
// Rendering (stubs — later increments)
// ---------------------------------------------------------------------------

HBITMAP DVP_LoadBitmap(HWND hWnd, LPTSTR lpszName,
                       LPVIEWERPLUGINFILEINFO lpVPFileInfo,
                       LPSIZE /*lpszDesiredSize*/, HANDLE /*hAbortEvent*/) {
  // Decode the first composited frame and hand Opus a bitmap. Opus does the
  // scaling (thumbnails) and zoom/pan itself on the still path.
  // (lpszDesiredSize is a thumbnail hint we ignore for now — full-size is fine.)
  const std::vector<uint8_t> bytes = animview::ReadWholeFile(lpszName);
  if (bytes.empty()) return nullptr;

  animview::Frame frame;
  if (!animview::DecodeFirstFrame(bytes.data(), bytes.size(), frame)) {
    return nullptr;
  }

  if (lpVPFileInfo != nullptr) {
    lpVPFileInfo->szImageSize.cx = frame.width;
    lpVPFileInfo->szImageSize.cy = frame.height;
    lpVPFileInfo->dwFlags |= DVPFIF_HasAlphaChannel;
  }

  return FrameToDIBSection(hWnd, frame);
}

HWND DVP_CreateViewer(HWND hWnd, LPRECT lpRc, DWORD dwFlags) {
  return animview::CreateViewerWindow(hWnd, lpRc, dwFlags);
}
