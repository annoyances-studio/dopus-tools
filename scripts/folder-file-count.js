// Directory Opus script add-in: FolderFileCount
//
// A "Files" column showing how many files each FOLDER contains (direct children),
// cached IN MEMORY so it survives navigation. Opus's native count recomputes on
// every folder re-read, so it's lost when you open a folder in the same lister
// (e.g. you forgot Ctrl). This caches by path for the session: a re-visit is an
// instant hash lookup, no re-count.
//
// Scope, on purpose: in-memory only (no disk persistence - that was fragile and,
// since a cold count is script-speed either way, it couldn't make a first-ever
// scan fast). So the cache resets when Opus restarts, and the win is purely
// re-visits within a session. If you change a folder's contents and want the new
// number, run `ClearFolderCountCache` (counts are path-keyed, not change-detected).
//
// Install: Preferences > Toolbars > Scripts > Add (point at this .js). Enable the
// "Files" column (right-click a column header, or Folder Options). Sort by it to
// group the folders that have files.

/* global DOpus, Script */

// Session cache: lowercased path -> file count. Script-global, so it persists
// across column calls and folder navigation for the life of the loaded script.
var g_cache = {};

function OnInit(initData) {
    initData.name = "FolderFileCount";
    initData.version = "2.0";
    initData.desc = "Cached per-folder file count (survives navigation)";
    initData.default_enable = true;
    initData.min_version = "12.0";

    var col = initData.AddColumn();
    col.name = "FileCount";
    col.method = "OnFileCountColumn";
    col.label = "Files";
    col.justify = "right";
    col.autogroup = false;

    var cmd = initData.AddCommand();
    cmd.name = "ClearFolderCountCache";
    cmd.method = "OnClearFolderCountCache";
    cmd.desc = "Forget cached folder file counts (forces a recount)";
    cmd.label = "Clear Folder Count Cache";
}

function countFiles(path) {
    var n = 0;
    try {
        var fe = DOpus.FSUtil.ReadDir(path);  // shallow
        while (fe && !fe.complete) {
            var fi = fe.next();
            if (fi && !fi.is_dir) n++;
        }
    } catch (e) {
        return -1;  // unreadable
    }
    return n;
}

function OnFileCountColumn(data) {
    var item = data.item;
    if (!item || !item.is_dir) { data.value = ""; return; }  // blank for files

    var key = String(item.realpath).toLowerCase();
    var hit = g_cache[key];
    if (hit !== undefined) {  // path hit: instant, no re-count (note: 0 is a valid count)
        data.value = String(hit);
        data.sort = hit;
        return;
    }

    var c = countFiles(String(item.realpath));
    if (c < 0) { data.value = "?"; return; }  // don't cache unreadable

    g_cache[key] = c;
    data.value = String(c);
    data.sort = c;
}

function OnClearFolderCountCache() {
    g_cache = {};
    DOpus.Output("FolderFileCount: cache cleared (recounts on next read).");
}
