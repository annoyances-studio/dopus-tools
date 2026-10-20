# animview — animated WebP (and, later, fixed GIF) for Directory Opus

A native Opus viewer plugin. See [`REVIEW.md`](REVIEW.md) for the architecture, the GIF-OOM diagnosis of the
fork base, and the decode plan.

**Status: increment #4 — WebP + GIF.** Plays animated **WebP** (libwebp) and **GIF** (WIC — no extra
dependency) in one viewer window, fit-to-window + zoom, thumbnails from the first frame. Only the current
composited canvas is ever held (O(1) memory) — the GIF out-of-memory of the old plugin is gone by design.
Deferred: pan/scrollbars when zoomed past the window.

**To replace the old GIF plugin:** both claim `.gif`, and **Opus loads whichever viewer plugin is ordered
first** (Preferences → Plugins → Viewer — drag `animview` above the old *Animated GIF* plugin, or disable/remove
`gifanim.dll`).

## Layout

```
core/   pure logic — WebP probe + libwebp version. No Windows, no Opus, no GDI. Unit-tested.
opus/   the Opus-facing DLL — DVP_* exports, and later the window + GDI (the thin, hard-to-test shell).
tests/  headless CTest over core/ — byte fixtures, no Opus, no GPU.
```

The split is deliberate: decode/compositing logic that can be reproduced in a test lives in `core/`; only the
genuinely Opus/GDI-bound glue lives in `opus/`. A "this .webp won't animate" report becomes a fixture in
`tests/`, not a remote-debug session.

## Prerequisites

- **Visual Studio 2026** with the *Desktop development with C++* workload (bundles CMake + Ninja).
- The **Opus Plugin SDK** on disk — download from **https://support.gpsoft.com.au/resources**. Point
  `-DOPUS_SDK_DIR=<path>` at the folder containing `headers/` (default `C:/Code/dopus-sdk/opus_sdk`). Not
  redistributed in this repo. **Without it, `core/` + the tests still build** (only the plugin DLL is skipped),
  which is what lets CI run.
- Network on first configure: libwebp (v1.4.0) is fetched and built from source via CMake `FetchContent`
  (no vcpkg needed).

## Build

From **Developer PowerShell for VS 2026** (so cmake/ninja/cl are on PATH):

```
cd dopus-plugins/animview
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-debug        # runs the core unit tests (configure x64-debug first if needed)
```

Or just **open the `animview` folder in VS 2026** — it reads `CMakePresets.json` automatically.

The plugin lands at `out/build/x64-release/opus/animview.dll`.

## Install into Opus

1. Copy `animview.dll` into the Opus install's **Viewers** directory (needs admin rights):
   `C:\Program Files\GPSoftware\Directory Opus\Viewers\animview.dll` — confirmed working on Opus 12 x64,
   2026-10-19. (The viewer plugin API is v4, which Opus 12 and 13 both load; the DLL must match Opus's
   bitness — 64-bit here.) The DLL goes straight in `Viewers`, not a per-plugin subfolder.
2. In Opus: **Preferences → Plugins → Viewer** → *Refresh* (or restart Opus). "WebP Animation" should appear.
3. Increment #1 check: the plugin lists as loaded. (It claims only animated `.webp`; still `.webp` stays with
   the built-in handler. Nothing displays yet — that's increment #2.)

## Roadmap

- **#1:** build spine, libwebp linked, identify animated WebP. ✅
- **#2:** `DVP_LoadBitmap` — decode the first frame via libwebp into a **DIBSection** (thumbnail + still). ✅
- **#3:** `DVP_CreateViewer` — owned window + `WebPAnimDecoder` playback loop (UI-thread `WM_TIMER`, not a
  worker thread); fit-to-window + zoom via the Opus `DVPLUGINMSG_*` messages; only the current composited
  canvas kept (O(1) memory), painted with `StretchDIBits` (no DDB). ✅
- **#4:** GIF via WIC behind a format-agnostic `AnimSource` (factory picks WebP/GIF by header); one plugin
  replaces Leo's and fixes its OOM. ✅
- **#3b (deferred):** pan/scrollbars when zoomed past the window; mouse-wheel/drag.
- **Before public repo:** add MPL headers (done) + `LICENSE`/`THIRD-PARTY` (done); keep the SDK a prerequisite.

## Continuous integration

CI builds `core/` + runs the full test suite on every push **without the SDK** (libwebp is fetched, WIC is OS)
— the logic worth gating. The plugin DLL link needs the SDK, so it's a local/release step. The workflow is at
the repo root: [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml).

## License & credits

MPL-2.0 — see the repo [`LICENSE`](../../LICENSE). Third-party components and reference credits (libwebp BSD,
WIC, the GPSoftware SDK, Leo Davidson's plugins as a studied reference) are in [`THIRD-PARTY.md`](THIRD-PARTY.md).
