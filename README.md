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

## License

MPL-2.0 — see [`LICENSE`](LICENSE). Per-plugin third-party notices live beside each plugin.
