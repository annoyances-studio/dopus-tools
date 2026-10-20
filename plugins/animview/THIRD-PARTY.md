# Third-party components & credits

**animview** is licensed under **MPL-2.0** (see the repo [`LICENSE`](../../LICENSE)). It builds on:

| Component | License | Notes |
|---|---|---|
| [libwebp](https://github.com/webmproject/libwebp) | BSD-3-Clause (Google) | Fetched via CMake and statically linked (WebP decode + animation). **Ship its `COPYING` with any binary you distribute.** |
| Windows Imaging Component (WIC) | Windows OS component | GIF decoding. No separate license. |
| [Directory Opus Plugin SDK](https://support.gpsoft.com.au/resources) | © GPSoftware | Required to build the **plugin DLL**. **Not redistributed here** — download it yourself and point `OPUS_SDK_DIR` at it. Used under GPSoftware's terms to build against; the SDK headers are never committed. |

## Credits / references (no code copied)

- **Leo Davidson** — his open-source Directory Opus plugins ([pretentiousname.com](https://pretentiousname.com/))
  were studied as a reference for the viewer-plugin message contract. animview's code is original; the OOM
  analysis of the old Animated GIF plugin is in [`REVIEW.md`](REVIEW.md).
- **GPSoftware** — for Directory Opus, the Plugin SDK, and its documentation.
