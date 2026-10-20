# dopus-tools

Tools that add functionality to **Directory Opus** — by [Annoyances Studio](https://github.com/annoyances-studio).

Small, ownable extensions for the one file manager with no FOSS peer worth the switch. The host is commercial;
these are plain, portable, MPL-licensed text you own — sovereignty at the layer that matters.

## Contents

### [`scripts/`](scripts/) — VBScript / JScript add-ins

Rename scripts, script columns, script commands. No build step; paste into the relevant DOpus dialog.

- **photo-date-folders.rename.vbs** — sort photos/videos into `YYYY-MM-DD\` folders by metadata date (with
  filename-date and file-time fallbacks).

### [`plugins/`](plugins/) — native C++ viewer plugins

Built against the GPSoftware **Opus Plugin SDK**.

- **[animview](plugins/animview/)** — animated **WebP** + **GIF** in one viewer window, **out-of-memory-free**.
  Replaces the old Animated GIF plugin (which OOMed frequently) and adds animated WebP, which Opus has never
  supported. WebP via libwebp, GIF via WIC (no extra dependency). Pure testable `core/` + a thin Opus/GDI shell.

## Building the plugins

Each plugin is self-contained (CMake + its own README). The **Opus Plugin SDK** is a prerequisite — download it
from https://support.gpsoft.com.au/resources and point `-DOPUS_SDK_DIR` at it. The plugin `core/` + its unit
tests build **without** the SDK (that's what CI runs); only the final DLL link needs it.

## Releasing

The plugin DLL is built locally (it needs the Opus SDK, so CI can't produce it) and published
via [`release.ps1`](release.ps1) — plain REST, no vendor CLI, so it ports across forges by editing one
settings block. Put a fine-grained PAT (Contents: write) in `$env:RELEASE_TOKEN`, then:

```powershell
$env:RELEASE_TOKEN = op read "op://Private/dopus-tools release/token"
./release.ps1 -Tag v0.1.0 -Notes "Animated WebP + GIF viewer for Directory Opus."
```

It builds the x64 Release DLL, creates the release (tagging `main`), and uploads `animview.dll` + the
libwebp notice.

## License

MPL-2.0 — see [`LICENSE`](LICENSE). Per-plugin third-party notices live beside each plugin.

## AI assistance

dopus-tools is developed openly with the help of Claude (Anthropic). We state this plainly: commits Claude
helped write carry a `Co-Authored-By: Claude` trailer. The code and design are open source so the work can be
inspected, reused, and given back. If you'd rather not use AI-assisted software, that's a fair call — this
disclosure is here so you can make it.
