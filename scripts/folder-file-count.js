// Directory Opus script add-in: FolderFileCount
//
// A "Files" column showing how many files each FOLDER contains (direct children),
// CACHED so it survives navigation. Opus's native count recomputes every time a
// folder is re-read, so it's lost when you navigate away and back (e.g. opening a
// folder in the same lister because you forgot Ctrl). This keeps a cache keyed by
// path + folder modified-time: a re-read returns instantly, and a folder only
// recounts when it actually changed. The cache is also persisted to disk so it
// survives closing Opus.
//
// Use case: scanning tens of thousands of folders to find the ones that have files.
// Sort by this column to group the non-empty ones.
//
// Install: Preferences > Toolbars > Scripts > Add (point at this .js). Enable the
// "Files" column (right-click a column header, or Folder Options). The command
// `ClearFolderCountCache` forces a full recount (after you've changed folders).

/* global DOpus, Script, ActiveXObject, Enumerator */

// In-memory cache: lowercased path -> { m: mtimeToken, c: count }. Script-global,
// so it persists across column calls and folder navigation for the whole session.
var g_cache = {};
var g_cacheFile = null;
var g_dirtySinceCompact = 0;

function OnInit(initData) {
    initData.name = "FolderFileCount";
    initData.version = "1.0";
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

    try { loadCache(); } catch (e) { DOpus.Output("FolderFileCount: loadCache failed: " + e); }
}

// A folder's modified-time changes when entries are added/removed, so it's our
// cache-invalidation key. D#/T# are DOpus's date/time format prefixes.
function mtimeToken(item) {
    try { return String(item.modify.Format("D#yyyyMMdd T#HHmmss")); }
    catch (e) {
        try { return String(item.modify); } catch (e2) { return ""; }
    }
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
    if (!item || !item.is_dir) { data.value = ""; return; }  // files/this blank

    var key = String(item.realpath).toLowerCase();
    var mkey = mtimeToken(item);

    var hit = g_cache[key];
    if (hit && hit.m === mkey) {
        data.value = String(hit.c);
        data.sort = hit.c;
        return;
    }

    var c = countFiles(String(item.realpath));
    if (c < 0) { data.value = "?"; return; }  // don't cache errors

    g_cache[key] = { m: mkey, c: c };
    appendEntry(key, mkey, c);  // best-effort persistence
    data.value = String(c);
    data.sort = c;
}

function OnClearFolderCountCache() {
    g_cache = {};
    try {
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var f = cacheFilePath();
        if (f && fso.FileExists(f)) fso.DeleteFile(f);
    } catch (e) { DOpus.Output("FolderFileCount: clear failed: " + e); }
    DOpus.Output("FolderFileCount: cache cleared. Re-read folders to recount.");
}

// ---- persistence (append-log of 'path|mtime|count'; last line per path wins) ---

function cacheFilePath() {
    if (g_cacheFile !== null) return g_cacheFile;
    g_cacheFile = "";
    try {
        var sh = new ActiveXObject("WScript.Shell");
        var dir = sh.ExpandEnvironmentStrings("%LOCALAPPDATA%") + "\\dopus-tools";
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        if (!fso.FolderExists(dir)) fso.CreateFolder(dir);
        g_cacheFile = dir + "\\folder-counts.txt";
    } catch (e) {
        DOpus.Output("FolderFileCount: no cache file (" + e + ") — in-memory only.");
    }
    return g_cacheFile;
}

function appendEntry(key, mkey, c) {
    try {
        var f = cacheFilePath();
        if (!f) return;
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var ts = fso.OpenTextFile(f, 8, true);  // 8 = append, create if missing
        ts.WriteLine(key + "|" + mkey + "|" + c);
        ts.Close();
    } catch (e) { /* non-fatal: cache still works in memory */ }
}

function loadCache() {
    var f = cacheFilePath();
    if (!f) return;
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    if (!fso.FileExists(f)) return;

    var lines = 0, unique = 0;
    var ts = fso.OpenTextFile(f, 1);  // read
    while (!ts.AtEndOfStream) {
        var line = ts.ReadLine();
        lines++;
        var p = line.split("|");
        if (p.length >= 3) {
            if (!g_cache[p[0]]) unique++;
            g_cache[p[0]] = { m: p[1], c: parseInt(p[2], 10) };  // last wins
        }
    }
    ts.Close();

    // Compact the append-log if it has bloated with duplicate re-counts.
    if (lines > unique * 2 + 100) compact();
}

function compact() {
    try {
        var f = cacheFilePath();
        if (!f) return;
        var fso = new ActiveXObject("Scripting.FileSystemObject");
        var ts = fso.OpenTextFile(f, 2, true);  // 2 = overwrite
        for (var k in g_cache) {
            ts.WriteLine(k + "|" + g_cache[k].m + "|" + g_cache[k].c);
        }
        ts.Close();
    } catch (e) { DOpus.Output("FolderFileCount: compact failed: " + e); }
}
