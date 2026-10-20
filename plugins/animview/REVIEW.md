# animview — code review of the fork base & architecture plan

**Goal:** one Directory Opus viewer plugin that shows **animated WebP** and **fixes animated GIF's
out-of-memory**, forked from Leo Davidson's open `gifanim` plugin with the decoder swapped to **libwebp**.

**Reviewed (2026-10-19):** `C:\Code\dopus-sdk\PNOpusPlugins_2009-08-03\gifanim` (Leo's source, 2009, VS2008)
against the SDK at `C:\Code\dopus-sdk\opus_sdk\headers` ("Opus 9 Viewer Plugins", API **version 4**). The v4
API is still what Opus 12/13 load (a current Opus-13 JPEG-XL viewer plugin uses the same `DVP_*` exports), so
the API surface is not the problem — the *age of the implementation* is.

---

## 1. How an Opus viewer plugin works (the contract)

A viewer plugin is a DLL exporting `DVP_*` functions (`viewer plugins.h`). Two ways to put pixels on screen,
and `gifanim` uses **both**:

- **Still / thumbnail path — `DVP_LoadBitmap` → returns one `HBITMAP`.** Opus owns the window and does
  zoom/pan/fit/slideshow itself. Single frame only (this is why other plugins "show only frame 1" of animated
  input). `gifanim` uses this for **thumbnails** (`gifanim.cpp:336` → `loadBitmapGifFile`).
- **Window-owning path — `DVP_CreateViewer` → returns an `HWND`.** The plugin creates its **own child window**
  and runs a timer loop to animate. This is the only path that animates. `gifanim.cpp:350` →
  `CViewerToolbarProxy::CreateViewerToolbarProxyWindow` → `CImageViewer`.

**Key reassurance on the "zoom will be interesting" worry:** on the window-owning path **Opus still drives
zoom/rotate/scroll** — it sends the plugin window messages (`viewer plugins.h:276` `DVPLUGINMSG_*`):
`SETZOOM`/`ZOOM`/`GETZOOMFACTOR`/`GETZOOMLIMITS`, `ROTATE`, `RESIZE`, `MOUSEWHEEL`, `REDRAW`, `CLEAR`,
`COPYSELECTION`, `PRINT`, context-menu, full-screen, etc. The plugin doesn't build a zoom UI — it **honors the
zoom factor when it paints** and reports its limits. Leo's `ViewerToolbarProxy` + `ImageViewer` +
`SelectBoxPainter` already implement that whole message contract. **We inherit working zoom/pan/selection and
only replace the decode + frame-buffer layer.**

Identify/registration is trivial and per-format: `DVP_Identify` (gifanim.cpp:211) sets handled extensions and
a plugin GUID; `DVP_IdentifyFileBytes` (gifanim.cpp:248) sniffs the magic bytes (`GIF87a`/`GIF89a`). For us:
new GUID, `.webp`, and the WebP magic (`RIFF....WEBP`, with `VP8X` + an `ANIM` chunk = animated).

---

## 2. Architecture of Leo's plugin

```
DVP_CreateViewer
  └─ CViewerToolbarProxy        (toolbar: play/pause/back/fwd, flatten mode)   ViewerToolbarProxy.cpp
       └─ CImageViewer          (the owned window: timer loop, paint, zoom)    ImageViewer.cpp
            ├─ CAbstractImageList   m_frames = vector<pair<CAbstractImage*,bool>>   AbstractImage.cpp
            │     └─ CAbstractImage (one frame)                                 AbstractImage.cpp
            │           ├─ CMemoryImage  → RGBQUAD* m_pPixelData (full canvas)  MemoryImage.h
            │           │     └─ CGifImage (decoded GIF frame)                  GifDecoder.*
            │           └─ Flat/Framed/Proxy/Tiled/ImageInImage (compositing)
            ├─ CFramePainter        (composites a frame for paint)             FramePainter.*
            ├─ CImagePainter        (blits to window, applies zoom)           ImagePainter.*
            ├─ CSelectBoxPainter    (rubber-band selection)                   SelectBoxPainter.*
            └─ CInfoBoxPainter      (on-image info overlay)                   InfoBoxPainter.*
```

`CGifFile` (GifDecoder.h:9) is the **only GIF-specific decode class** — LZW reader, color map, extensions,
disposal. `CGifImage::LoadGifImage` (GifDecoder.h:74) builds the whole `CAbstractImageList`. **This is the
seam to replace with libwebp;** everything above `CAbstractImageList` is format-agnostic and stays.

---

## 3. The OOM — two distinct, confirmed mechanisms

### Bug A — decode-time RAM: all frames resident, each a full-canvas 32-bit buffer
- Each frame is a `CMemoryImage` owning a full `RGBQUAD *m_pPixelData` (MemoryImage.h:49) — 4 bytes/pixel.
- `CGifImage::setPixel` writes across the whole `m_iWidth*m_iHeight` (GifDecoder.h:171) and GIF disposal is
  handled by **inheriting the previous frame's full buffer** (`inheritImageBuffer`, GifDecoder.h:115). So a
  GIF's delta-frames each become a **full-logical-screen** RGBA buffer.
- `LoadGifImage` decodes **every frame up front** into `CAbstractImageList::m_frames` and keeps them all for
  the image's lifetime. The telltale: an out-param `pbNotAllFramesLoadedDueToMemory` (GifDecoder.h:75) — Leo
  already hits allocation failure and bails mid-load.
- Cost ≈ `width × height × 4 × frameCount`, resident. Big-dimension or many-frame files dominate here. On
  64-bit this needs to be large to OOM — so this is the *second* trigger, not the "few frames" one.

### Bug B — paint-time GDI pool: the interactive cache uses a DDB, not a DIB (the "few frames" killer)
- The still/thumbnail/clipboard/wallpaper paths correctly allocate with **`CreateDIBSection`** — a DIB in the
  **process address space** (AbstractImage.cpp:705, :875, :906). On 64-bit that's effectively unbounded.
- But the **interactive animated paint cache** (`GeneratePaintCache`) allocates with
  **`CreateCompatibleBitmap`** (AbstractImage.cpp:361 and :396) — a **device-dependent bitmap (DDB)** that
  lives in the **limited GDI / display-driver pool, not your RAM** — and it's sized to the **zoomed**
  dimensions (`pImageSize`). Zoom in on a GIF and each cached frame becomes a huge DDB in a capacity-limited
  pool; a few large/zoomed frames exhaust it → "out of memory," **and 64-bit RAM can't help** because that's
  not where the allocation lives. This matches "OOMs frequently, even with a few frames" exactly.
- `DeleteCachedPaintBitmaps(keep current, keep next)` (AbstractImage.cpp:225) trims to two frames, but (a) at
  high zoom even two zoomed DDBs is plenty to exhaust the pool, and (b) the churn of regenerating them every
  zoom/redraw thrashes the pool.

### Fix pattern for the rewrite (kills both)
1. **Don't hold all frames.** `WebPAnimDecoder` yields frames sequentially already composited with disposal +
   blend applied — keep only the **current composited canvas** (plus optionally the next). Memory becomes
   O(1 canvas), not O(frames).
2. **DIBSection everywhere, never `CreateCompatibleBitmap`.** Keep the backing canvas as a top-down 32-bit
   `CreateDIBSection` (app memory). The thumbnail path already proves this works in the plugin.
3. **Scale on blit, don't cache zoomed bitmaps.** `SetStretchBltMode(HALFTONE)` + `StretchDIBits` straight to
   the window DC at the current zoom, rather than materializing a full zoomed DDB per frame.
4. **Honor `DVPLUGINMSG_CLEAR`** (and window destroy) to free the canvas.

---

## 4. The libwebp decode seam

Replace `CGifFile`/`CGifImage` with a WebP decoder that fills the same `CAbstractImageList` shape (or, better,
a new streaming source that yields one composited canvas at a time):

- **Animated path:** `WebPAnimDecoder` from **`demux.h`** (NOT plain `WebPDecode`, which returns
  `VP8_STATUS_UNSUPPORTED_FEATURE` on animated files and won't even give frame 1). `WebPAnimDecoderGetNext`
  hands back an already-composited RGBA canvas + a timestamp; per-frame duration = delta of timestamps. Loop
  count from `WebPAnimInfo`.
- **Still WebP:** already shipped with Opus — our plugin should **only register for animated** WebP (detect
  the `ANIM`/`ANMF` chunks in `DVP_IdentifyFileBytes`) so it doesn't fight the built-in still handler. If an
  animated file is detected, we handle it; otherwise return FALSE and let Opus's still viewer take it.
- **libwebp** via vcpkg: `vcpkg install libwebp` (brings `webp` + `webpdemux`), static link.

---

## 5. Modernization needed (it's 2009 code)

- **Build files:** `gifanim.vcproj` / `.sln` are VS2008. Regenerate as a modern **`.vcxproj`** (or CMake),
  **x64**, Unicode, `/std:c++17`, current Windows SDK. The SDK on disk is headers-only + a VS6-era `.dsp`
  sample — we write our own project.
- **Dependencies:** the plugin leans on `common/LeoHelpers.{h,cpp}` and `common/Win32IOWrapper.*` (his utility
  lib) and `stdafx.h` (old precompiled-header + ATL/_T macros). Decide: vendor a trimmed `LeoHelpers` subset,
  or cut it. It's large — only pull what `animview` actually calls.
- **Raw GDI + manual `new(std::nothrow)`/delete** throughout. Acceptable, but the rewrite is a chance to wrap
  HBITMAP/HDC in RAII and drop the dead code we won't use (print, wallpaper, sprocket thumbnails, dithering,
  gamma, tiled/flatten modes) unless we want them.
- **TLS plumbing** (gifanim.cpp:104 `getGifViewerTlsData`, DllMain TLS) — keep; it's how per-thread viewer
  state is tracked and it's correct.

---

## 6. License (decided 2026-10-19: **MPL 2.0**)

We did **not** fork Leo's source — the viewer shell and core were written clean against the SDK, using his
plugin only as a *reference*, and GIF decoding is **WIC** (not his decoder). So no terms flow in from his code,
and **MPL 2.0** (Daniel's usual) is free to apply. Conditions, all satisfiable:

- **libwebp** is BSD-3-Clause (permissive; static-links fine under MPL). **WIC** is a Windows OS component (no
  burden). Both compatible with MPL 2.0 (file-level copyleft).
- The **GPSoftware Plugin SDK** is used to build but **never redistributed** (external at `C:\Code\dopus-sdk`,
  not committed), so its terms don't bind the repo — it's a documented prerequisite.
- **Publish-time checklist:** MPL header on each source + a `LICENSE`; a `THIRD-PARTY` notice bundling
  libwebp's BSD license with any distributed binary (WIC = OS); keep the SDK a prerequisite; courtesy credit to
  Leo Davidson + GPSoftware as references.

(Build-approach note, now settled: we went **clean reimplementation**, not "fork Leo and prune" — simpler to
license and to maintain, and WIC removed the need for his GIF decoder entirely.)

---

## Changelog
- 2026-10-19: Initial review. Confirmed the two OOM mechanisms (all-frames-resident full-canvas buffers;
  DDB paint-cache in the GDI pool) with file:line evidence; mapped the decode seam to `WebPAnimDecoder`;
  listed modernization + the license gate. Opus is 64-bit; SDK is viewer API v4 (still current).
