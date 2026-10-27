# CLAUDE.md — context for Claude Code sessions in dopus-tools

Read this first. Small, ownable extensions for **Directory Opus** (the commercial file manager), by
**Annoyances Studio**. Two kinds of extension, one repo:

- **`scripts/`** — VBScript / JScript DOpus add-ins (rename scripts, script columns, script commands).
  **No build step** — paste into the relevant DOpus dialog or save as a preset.
- **`plugins/`** — native **C++** viewer plugins built against the GPSoftware **Opus Plugin SDK**.
  Currently **animview** (animated WebP + GIF in one viewer, out-of-memory-free).

The host is commercial and closed; these extensions are plain, portable, **MPL-2.0** text you own — sovereignty
at the layer that matters. Repo: `github.com/annoyances-studio/dopus-tools`.

## Who / what

- Owner: Daniel — DOpus **12** power user (the DLL also loads on 13), extends it via scripts + native plugins.
  Comfortable with CLI, C++, CMake, and Windows internals; be specific, skip the basics.
- Target: Directory Opus **12/13**, **x64**. Viewer plugin API **v4**.

## Layout

```
scripts/                 VBScript/JScript add-ins (no build) + README with the DOpus gotchas log
plugins/animview/        native viewer plugin (animated WebP via libwebp + GIF via WIC)
  core/                  pure logic — no Windows / no Opus / no GDI. Unit-tested.
  opus/                  the Opus-facing DLL — DVP_* exports + window + GDI (the thin shell)
  tests/                 headless CTest over core/ (byte fixtures, no Opus, no GPU)
  REVIEW.md              architecture + the GIF-OOM diagnosis of the old plugin + decode plan
  THIRD-PARTY.md         third-party notices + reference credits
release.ps1              REST-based GitHub release (portable; no vendor CLI)
.github/workflows/ci.yml CI: builds core/ + runs the tests WITHOUT the SDK
```

`plugins/animview/out/` is build output (gitignored) — don't read it; it's thousands of CMake/libwebp files.

## The load-bearing convention: testable core / thin shell

Every native plugin splits **pure, testable logic (`core/`)** from the **thin, hard-to-test Opus/GDI shell
(`opus/`)**. `core/` has no Windows, no Opus, no GDI, and is covered by headless CTest. The rule in practice:
a *"this file won't animate / renders wrong"* report becomes a **fixture in `tests/`**, not a remote-debug
session. Keep new logic in `core/`; keep `opus/` as glue. **Tests exist from increment #1, not bolted on later.**

Same spirit for scripts: when a DOpus API quirk bites, **record it in `scripts/README.md`** ("gotchas learned so
we don't relearn them") rather than just fixing it silently. E.g. DOpus `Date` objects need
`Day()`/`Month()`/`Year()` coercion (not `.Format()`), and script columns can't beat Opus's native speed on a cold
pass — only cache re-visits.

## Building a plugin (animview)

- **Prereqs:** Visual Studio **2026** + *Desktop development with C++* (bundles CMake + Ninja). First configure
  needs network — **libwebp v1.4.0** is fetched via CMake `FetchContent` (no vcpkg).
- **The Opus SDK is only needed for the final DLL link.** `core/` + `tests/` build **without** it — that's exactly
  what CI gates. Download the SDK from https://support.gpsoft.com.au/resources and point
  `-DOPUS_SDK_DIR=<folder containing headers/>` (default `C:/Code/dopus-sdk/opus_sdk`).
  **Never commit or redistribute the SDK.**
- From **Developer PowerShell for VS 2026** (so cmake/ninja/cl are on PATH):
  ```
  cmake --preset x64-release
  cmake --build --preset x64-release
  ctest --preset x64-debug        # core unit tests (configure x64-debug first if needed)
  ```
  The DLL lands at `plugins/animview/out/build/x64-release/opus/animview.dll`. Or just open the `animview` folder
  in VS 2026 — it reads `CMakePresets.json`.
- **Install:** copy the DLL into the Opus install's **Viewers** dir
  (`C:\Program Files\GPSoftware\Directory Opus\Viewers\`, admin), then Preferences → Plugins → Viewer → Refresh.
  For GIF, order it **above** the old *Animated GIF* plugin (Opus uses whichever viewer is listed first).

## Releasing

The DLL is built locally (it needs the SDK, so CI can't produce it) and published via **`release.ps1`** — plain
REST, no vendor CLI, portable across forges by editing one settings block. Token from 1Password:

```powershell
$env:RELEASE_TOKEN = op read "op://Private/dopus-tools release/token"
./release.ps1 -Tag v0.1.0 -Notes "..."
```

Builds the x64 Release DLL, tags `main`, uploads `animview.dll` + the libwebp notice.
`release.ps1` must stay **PS 5.1-safe → ASCII only** (an em-dash / UTF-8-no-BOM once broke its parse).

## Conventions

- **Git:** plain git, commit + push **direct to `main`** (no PR flow). Commit messages are scoped:
  `animview: …`, `scripts(FolderFileCount): …`, `release.ps1: …`.
- **Commit trailer:** end commits Claude helped write with a `Co-Authored-By: Claude <noreply@anthropic.com>`
  trailer, naming the model actually used (e.g. `Claude Opus 5.5`) — never copy a model name from older commits.
  The README's AI-assistance disclosure commits to this trailer.
- **License:** MPL-2.0 (`LICENSE`). Per-plugin third-party notices live beside each plugin (`THIRD-PARTY.md`).
- **Provenance:** Leo Davidson's Opus plugins were **studied as a reference, not forked** — the viewer shell/core
  were written clean against the SDK. Keep it that way; credit lives in `THIRD-PARTY.md`.
- Markdown is GitHub-flavored.

## How to be useful here

- Check the relevant `README.md` / `REVIEW.md` first — the design, the OOM diagnosis, and the SDK notes are
  already written down.
- Put logic where it can be tested (`core/`); turn a bug into a fixture before fixing it.
- Don't suggest vcpkg (libwebp is FetchContent) or redistributing the SDK.
- This is a **separate repo** from the homelab tree and from Margin — stay within it.
