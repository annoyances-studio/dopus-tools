# Directory Opus scripts

Daniel's **Directory Opus 12** customizations — rename scripts, script columns, script commands — versioned
here so they survive a config loss (the backed-up config is the belt; this is the suspenders) and so a lesson
learned in one accumulates for the next.

**Why they live in a commercial tool's repo:** DOpus is the one app with no FOSS peer worth the switch — but
the *scripts* are plain, portable, ownable text (sovereign at the layer that matters), so extending it this way
stays in keeping with the rest of the lab.

## Scripts

| File | Type | What it does |
|---|---|---|
| [`photo-date-folders.rename.vbs`](photo-date-folders.rename.vbs) | Rename (`OnGetNewName`) | Sorts photos/videos into `YYYY-MM-DD\` folders. Date priority: **metadata** (date-taken / release-date) → **date embedded in the filename** (`YYYYMMDD_`, strips `IMG_`/`PXL_`) → the **older of file created/modified** → **today** (last resort). Perfect for a fast phone-photo dump. |

## DOpus scripting gotchas learned (so we don't relearn them)

- **`item.create` / `item.modify` return DOpus `Date` objects**, not VBScript dates. Their `.Format()` uses
  non-obvious codes (gave garbled output with `yyyyMMddHHmmss`). **`Day()` / `Month()` / `Year()` coerce them
  to real VBScript dates** — use those (same as metadata date fields). Build a `yyyymmdd` numeric key to compare
  two dates reliably (no object `<`, no `.Format`).
- **Recovered files carry bogus timestamps** (NTFS 1601 / epoch 1970 "not set") → floor at year 1990 before
  trusting created/modified.

## Ideas / backlog (scriptable itches)

- **"Real Type" script column** — read each file's first bytes, show its *actual* type (magic-byte detection);
  the one-click answer to identifying recovered GUID-named blobs.
- **"Slideshow (sorted)" command** — collect a *selected* (unopened) folder's images, sort them explicitly, and
  `Show SLIDESHOW` on that order (fixes the weird default order when you slideshow a folder without opening it).
- **Cached file-count column** — persist per-folder file counts (keyed by path + mtime) so navigating in/out
  doesn't throw away a count of thousands of subfolders; recompute only when the folder actually changed.

## Native viewer plugin — PLANNED (next task, 2026-10-19)

Unlike the scripts above, a viewer is a **native C++ DLL** against the **DOpus Plugin SDK** (from GPSoftware's
downloads) — the one DOpus extension that's real engineering, not scripting.

**Goal (corrected 2026-10-19):** **animated WebP** in the viewer pane + **fix animated GIF's out-of-memory**.
Static WebP already displays — a non-animated WebP plugin *ships with Opus* — so the job is **animation only**,
not stills. GIF animation today is **Leo Davidson's open-source Animated GIF plugin** (pretentiousname.com),
which **OOMs frequently, even with a few frames**. GPSoftware's standing answer for animated WebP is: *the GIF
plugin source is available, write your own from it.* So: **one plugin, forked from the GIF source**, that (a)
fixes the GIF memory bug and (b) adds a WebP decode path — GIF + animated WebP in one DLL.

**The architecture is forced by the SDK (confirmed from GPSoftware docs/forum):** a viewer plugin has two
shapes —
- **Simple / still:** hand Opus one `HBITMAP`; Opus owns zoom/pan/slideshow. **Single frame only** — this is
  why the JPEG XL plugin shows "only the first frame" on animated input. No good for us.
- **Window-owning:** the plugin creates its **own child window** and does everything itself — the frame-timer
  playback loop **and** zoom/pan/fit. The *only* path that animates, and what Leo's GIF plugin already is.
- **Consequence (Daniel's instinct, confirmed):** because we own the window, **zoom/pan/fit are ours to
  implement** — they don't come free like the still path. The win of forking Leo: his plugin **already solved
  the window + zoom + timer loop for the animated case** — we keep that and swap the decoder.

**Decode (the real swap):** plain `WebPDecode` *won't even return frame 1* of an animated file
(`VP8_STATUS_UNSUPPORTED_FEATURE`). Animated WebP needs libwebp's **demux / animation API**
(`WebPAnimDecoder`, `WebPDemux` in `demux.h`), which walks frames + per-frame duration + disposal/blend. That's
the piece that replaces the GIF decoder.

**The GIF OOM — diagnose in Leo's source first (don't inherit it):** prime suspects — decoding **every frame to
a full-size 32-bit DIB up front and holding them all**, and/or **not freeing** them across image navigation
(a leak), made fatal if Opus is the **32-bit** build (2–4 GB address space + fragmentation → OOM on "a few"
large frames). Fix pattern: keep **one composited canvas + the current frame**, honor GIF disposal methods
instead of materializing N full canvases, free on unload. The WebP path should be built the same way from day
one. (Check Opus bitness: Help → About — it decides the OOM likelihood *and* the DLL target.)

**Dependencies to pull (downloads — Daniel grabs these; I can't fetch them):**
1. **Opus Plugin SDK** — GPSoftware downloads page (headers, sample viewer plugins, the real API surface). THE
   dependency; the plugin entry points get written against its samples, not guessed.
2. **Leo's Animated GIF plugin source** — pretentiousname.com (`pnop_source`) — the fork base.
3. **libwebp** — via vcpkg (`vcpkg install libwebp`) for the demux/anim decoder; clean static link.

**Build/repo plan (Daniel):** start coding **here in the homelab repo** (its own `dopus-plugins/` tree, separate
from these scripts); if it works, **move to its own repo**. It may grow into several plugins → structure so
**each plugin compiles independently** (per-plugin build, so a user builds just the one they want). Needs MSVC +
the DOpus SDK + libwebp; iterate-in-Opus testing. Match the **DLL bitness to the installed Opus**.

**References (readable, modern, on GitHub):** [`hjtgh/dopus_jxl_viewer`](https://github.com/hjtgh/dopus_jxl_viewer)
— a current, minimal C++ *still* viewer plugin for Opus 13 (clean skeleton: project layout, SDK entry points,
decoder linkage); [`HyperWorX/DopusWorX`](https://github.com/HyperWorX/DopusWorX) — a WebView2 doc-viewer DLL;
[`mrfearless/MOSViewer`](https://github.com/mrfearless/MOSViewer). **Rejected alternative:** a WebView2/Chromium
view (or the existing Metaplugin + Edge route) *does* animate WebP for free, but it's a whole browser per view
and won't integrate with Opus's native zoom/slideshow/fit — wrong fit for an image viewer.

## Using them

Rename scripts: Rename dialog → set type to **Script** → paste (or save as a rename **preset**). Columns/commands:
Preferences → Toolbars/Scripts, or **Settings → Scripts** → add a script add-in. Big API + version quirks →
sanity-check uncommon calls against the GPSoftware docs / resource.dopus.com.
