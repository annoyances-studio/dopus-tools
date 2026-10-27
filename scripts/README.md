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
| [`sorted-slideshow.js`](sorted-slideshow.js) | Command (`SortedSlideshow`) | Slideshow the selected folders' images in **natural, numeric-aware order** (`2` before `10`; `1.jpg`/`001.jpg` deterministic), full-screen. Fixes the cold-selection case where Opus uses the filesystem's enumeration order — unsorted on **exFAT** (no sorted directory index, unlike NTFS). |
| [`folder-file-count.js`](folder-file-count.js) | Column (`Files`) + command (`ClearFolderCountCache`) | Per-folder **file count**, cached **in memory** so re-visits within a session are instant (Opus's native count recomputes on every re-read, so it's lost when you open a folder in the same lister). Path-keyed; resets on restart, and `ClearFolderCountCache` forces a recount. Sort by it to group folders that have files. (Custom columns are script-only in Opus, so a *cold* count can't beat the native counter — the win is re-visits.) |

## DOpus scripting gotchas learned (so we don't relearn them)

- **`item.create` / `item.modify` return DOpus `Date` objects**, not VBScript dates. Their `.Format()` uses
  non-obvious codes (gave garbled output with `yyyyMMddHHmmss`). **`Day()` / `Month()` / `Year()` coerce them
  to real VBScript dates** — use those (same as metadata date fields). Build a `yyyymmdd` numeric key to compare
  two dates reliably (no object `<`, no `.Format`).
- **Recovered files carry bogus timestamps** (NTFS 1601 / epoch 1970 "not set") → floor at year 1990 before
  trusting created/modified.
- **Custom columns are script-only, so a cold count can't beat Opus's native one.** Any script column runs at
  script speed; the only win a cache can offer is *re-visits* (Opus's native folder count recomputes on every
  re-read). Don't chase a faster first pass (FolderFileCount).
- **`FSO.Files.Count` is slower than `DOpus.FSUtil.ReadDir`**, not faster — tried as an "optimization" and
  reverted. Prefer `ReadDir` for counting.
- **`FSO.OpenTextFile` defaults to ASCII** and silently mangles Unicode paths (folders then never match on
  reload — looks like "the cache doesn't work"). Pass format `-1` (Unicode) when reading/writing.
- **Disk persistence for a column cache wasn't worth it.** Append-log + Unicode/BOM handling + external edits made
  it fragile, and a per-call mtime check (`Date.Format`) cost more than it saved. FolderFileCount now keeps a plain
  in-memory `path -> count` map for the session; `ClearFolderCountCache` forces a recount.

## Ideas / backlog (scriptable itches)

- **"Real Type" script column** — read each file's first bytes, show its *actual* type (magic-byte detection);
  the one-click answer to identifying recovered GUID-named blobs.

## Native plugins

The C++ viewer plugins live under [`../plugins`](../plugins) — see
[`../plugins/animview`](../plugins/animview) (animated WebP + GIF, out-of-memory-free). Its
[`REVIEW.md`](../plugins/animview/REVIEW.md) has the GIF-OOM diagnosis and the SDK notes.

## Using them

Rename scripts: Rename dialog → set type to **Script** → paste (or save as a rename **preset**). Columns/commands:
Preferences → Toolbars/Scripts, or **Settings → Scripts** → add a script add-in. Big API + version quirks →
sanity-check uncommon calls against the GPSoftware docs / resource.dopus.com.
