// Directory Opus script add-in: SortedSlideshow
//
// Slideshow the images in the SELECTED folders (and any selected image files) in
// natural, numeric-aware order — so "2" sorts before "10", and padded vs unpadded
// numbers (1.jpg vs 001.jpg) order sensibly. This fixes the cold-selection case:
// Opus slideshows a selected-but-unopened folder in the filesystem's enumeration
// order, which on exFAT (no sorted directory index, unlike NTFS) is effectively
// unsorted. We enumerate + sort ourselves, then hand that order to Show SLIDESHOW.
//
// Order: selected top-level items are natural-sorted by name; each folder expands
// to its images (natural-sorted); selected loose images are included in place.
// e.g. folders a, b, c  ->  a/1.jpg a/2.png a/3.jpg  b/001.jpg b/002.webp  c/a.jpg c/b.png
//
// Install: Preferences > Toolbars > Scripts > Add (point at this .js). Then invoke
// the command `SortedSlideshow` from a toolbar button or hotkey.

/* global DOpus, Script */

function OnInit(initData) {
    initData.name = "SortedSlideshow";
    initData.version = "1.0";
    initData.desc = "Natural-sorted slideshow of selected folders' images";
    initData.default_enable = true;
    initData.min_version = "12.0";

    var cmd = initData.AddCommand();
    cmd.name = "SortedSlideshow";
    cmd.method = "OnSortedSlideshow";
    cmd.desc = "Slideshow the selected folders' images in natural (numeric-aware) order";
    cmd.label = "Sorted Slideshow";
    cmd.hide = false;
}

var IMAGE_EXTS = " jpg jpeg jpe jfif png gif webp bmp dib tif tiff avif jxl heic heif ico ";

function isImage(name) {
    var m = /\.([a-z0-9]+)$/i.exec(String(name));
    if (!m) return false;
    return IMAGE_EXTS.indexOf(" " + m[1].toLowerCase() + " ") >= 0;
}

// Natural comparator: split into digit / non-digit runs and compare run by run.
// Numeric runs compare by magnitude (leading zeros stripped), then length, then
// lexically — so 2 < 10, and 001 and 1 stay deterministic and adjacent.
function naturalCompare(a, b) {
    var re = /(\d+|\D+)/g;
    var aa = String(a).toLowerCase().match(re) || [];
    var bb = String(b).toLowerCase().match(re) || [];
    var n = Math.min(aa.length, bb.length);
    for (var i = 0; i < n; i++) {
        var x = aa[i], y = bb[i];
        if (x === y) continue;
        var xn = x.charCodeAt(0) >= 48 && x.charCodeAt(0) <= 57;
        var yn = y.charCodeAt(0) >= 48 && y.charCodeAt(0) <= 57;
        if (xn && yn) {
            var sx = x.replace(/^0+(?=\d)/, "");  // strip leading zeros, keep >=1 digit
            var sy = y.replace(/^0+(?=\d)/, "");
            if (sx.length !== sy.length) return sx.length < sy.length ? -1 : 1;  // magnitude
            if (sx !== sy) return sx < sy ? -1 : 1;
            if (x.length !== y.length) return x.length < y.length ? -1 : 1;      // 1 vs 001
        } else {
            return x < y ? -1 : 1;
        }
    }
    return aa.length - bb.length;
}

// Return the folder's image Items, natural-sorted by name. We keep the Items from
// the directory scan (they already carry full info) so the slideshow never has to
// re-stat each file — that per-file GetItem was the slow part on big folders.
function imagesInFolder(path) {
    var pairs = [];
    try {
        var fe = DOpus.FSUtil.ReadDir(path);  // shallow
        while (fe && !fe.complete) {
            var fi = fe.next();
            if (fi && !fi.is_dir && isImage(String(fi.name))) {
                pairs.push({ item: fi, name: String(fi.name) });  // read name once
            }
        }
    } catch (e) {
        DOpus.Output("SortedSlideshow: cannot read " + path + " (" + e + ")");
    }
    pairs.sort(function (a, b) { return naturalCompare(a.name, b.name); });
    var items = [];
    for (var i = 0; i < pairs.length; i++) items.push(pairs[i].item);
    return items;
}

function OnSortedSlideshow(scriptCmdData) {
    var func = scriptCmdData.func;
    var tab = func.sourcetab;
    if (!tab) return;

    // Top-level selected items we care about (folders + loose images), natural-sorted.
    var sel = [];
    var en = new Enumerator(tab.selected);
    for (; !en.atEnd(); en.moveNext()) {
        var it = en.item();
        if (it.is_dir || isImage(String(it.name))) sel.push(it);
    }
    sel.sort(function (x, y) { return naturalCompare(String(x.name), String(y.name)); });

    // Expand to the final ordered list of Items (each folder's images inline).
    var ordered = [];
    for (var i = 0; i < sel.length; i++) {
        if (sel[i].is_dir) {
            var items = imagesInFolder(String(sel[i].realpath));
            for (var j = 0; j < items.length; j++) ordered.push(items[j]);
        } else {
            ordered.push(sel[i]);  // loose selected image — already an Item
        }
    }

    if (ordered.length === 0) {
        DOpus.Output("SortedSlideshow: no images in the selection.");
        return;
    }

    // Hand the explicit order to the viewer. The Items come straight from the scan /
    // selection, so there's no per-file re-stat. FULLSCREEN opens it full-screen.
    var cmd = func.command;
    cmd.ClearFiles();
    for (var k = 0; k < ordered.length; k++) cmd.AddFile(ordered[k]);
    cmd.RunCommand("Show SLIDESHOW FULLSCREEN");
}
