// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "viewer.h"

#include <objidl.h>
#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "viewer plugins.h"

#include "animview/anim_source.h"
#include "animview/decoder.h"
#include "file_io.h"

namespace animview {
namespace {

const wchar_t kClassName[] = L"animview.webp.viewer";
constexpr UINT_PTR kTimerId = 1;       // frame animation
constexpr UINT_PTR kHideTimerId = 2;   // auto-hide the playback bar
constexpr int kMinFrameMs = 10;
constexpr int kZoomMin = 1;
constexpr int kZoomMax = 1600;
constexpr int kBarHeight = 40;
constexpr int kBtnW = 36;
constexpr int kHideDelayMs = 2500;

// Context-menu command IDs (our File > animview submenu items).
enum { CMD_PLAYPAUSE = 1, CMD_PREV = 2, CMD_NEXT = 3 };

struct ViewerState {
  HWND hwnd = nullptr;
  std::unique_ptr<AnimSource> source;  // WebP or GIF
  std::vector<uint8_t> composited;  // opaque BGRA canvas (alpha blended over bg)
  SIZE canvas{0, 0};
  size_t frame_count = 0;
  int zoom_req = ZOOM_FITPAGE;  // ZOOM_FITPAGE / ZOOM_ORIGINAL / positive percent
  COLORREF bg = RGB(0, 0, 0);
  bool has_image = false;

  // Pan / scroll (only meaningful when the displayed image exceeds the client).
  SIZE offset{0, 0};  // displayed-space pixels hidden off the top-left
  bool hbar = false, vbar = false;  // which scrollbars we currently show
  bool updating_scrollbars = false;
  bool panning = false;
  POINT pan_anchor{0, 0};
  SIZE pan_anchor_offset{0, 0};

  // Playback.
  bool paused = false;
  int cur_index = -1;  // frame currently displayed (-1 = none yet)
  DVPCONTEXTMENUITEM menu_items[9]{};
  std::wstring menu_labels[9];

  // Auto-hiding playback bar (animations only).
  bool bar_visible = false;
  bool mouse_tracking = false;  // TrackMouseEvent armed (for WM_MOUSELEAVE)
  bool scrubbing = false;       // dragging the progress bar
  int scrub_x = 0;              // cursor x while scrubbing (for a smooth thumb)
};

ViewerState* GetState(HWND hWnd) {
  return reinterpret_cast<ViewerState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
}

// Blend a straight-alpha BGRA frame over the background into an opaque BGRA buffer.
void CompositeOverBg(ViewerState* s, const Frame& f) {
  const size_t n = static_cast<size_t>(f.width) * f.height;
  s->composited.resize(n * 4);
  const uint8_t bb = GetBValue(s->bg), bg_g = GetGValue(s->bg), br = GetRValue(s->bg);
  const uint8_t* src = f.bgra.data();
  uint8_t* dst = s->composited.data();
  for (size_t i = 0; i < n; ++i, src += 4, dst += 4) {
    const unsigned a = src[3];
    const unsigned ia = 255 - a;
    dst[0] = static_cast<uint8_t>((src[0] * a + bb * ia) / 255);
    dst[1] = static_cast<uint8_t>((src[1] * a + bg_g * ia) / 255);
    dst[2] = static_cast<uint8_t>((src[2] * a + br * ia) / 255);
    dst[3] = 255;
  }
}

int FrameCountI(const ViewerState* s) {
  return static_cast<int>((std::max)(static_cast<size_t>(1), s->frame_count));
}

void ShowFrame(ViewerState* s, const Frame& f) {
  CompositeOverBg(s, f);
  s->has_image = true;
  InvalidateRect(s->hwnd, nullptr, FALSE);
}

// Timer-driven advance to the next frame; re-arms unless paused.
void AdvanceFrame(ViewerState* s) {
  Frame f;
  if (!s->source || !s->source->NextFrame(f)) return;
  s->cur_index = (s->cur_index + 1) % FrameCountI(s);
  ShowFrame(s, f);
  if (!s->paused && s->frame_count > 1) {
    SetTimer(s->hwnd, kTimerId, static_cast<UINT>(std::max(kMinFrameMs, f.duration_ms)),
             nullptr);
  }
}

void PausePlayback(ViewerState* s) {
  s->paused = true;
  KillTimer(s->hwnd, kTimerId);
}

void ResumePlayback(ViewerState* s) {
  s->paused = false;
  if (s->frame_count > 1) SetTimer(s->hwnd, kTimerId, 1, nullptr);  // next tick advances
}

void StepNext(ViewerState* s) {
  PausePlayback(s);
  Frame f;
  if (!s->source || !s->source->NextFrame(f)) return;
  s->cur_index = (s->cur_index + 1) % FrameCountI(s);
  ShowFrame(s, f);
}

void StepPrev(ViewerState* s) {
  PausePlayback(s);
  if (!s->source) return;
  const int fc = FrameCountI(s);
  const int idx = (s->cur_index - 1 + fc) % fc;
  Frame f;
  if (s->source->SeekToFrame(static_cast<size_t>(idx), f)) {
    s->cur_index = idx;
    ShowFrame(s, f);
  }
}

// Displayed image size for the current zoom + client size.
SIZE ComputeDisplayed(const ViewerState* s, const RECT& client) {
  SIZE out{1, 1};
  if (s->canvas.cx <= 0 || s->canvas.cy <= 0) return out;
  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;

  if (s->zoom_req == ZOOM_FITPAGE) {
    const double sx = cw > 0 ? static_cast<double>(cw) / s->canvas.cx : 1.0;
    const double sy = ch > 0 ? static_cast<double>(ch) / s->canvas.cy : 1.0;
    const double scale = (std::min)(sx, sy);
    out.cx = (std::max)(1L, static_cast<LONG>(s->canvas.cx * scale + 0.5));
    out.cy = (std::max)(1L, static_cast<LONG>(s->canvas.cy * scale + 0.5));
  } else {
    int pct = (s->zoom_req == ZOOM_ORIGINAL) ? 100 : s->zoom_req;
    pct = (std::max)(kZoomMin, (std::min)(kZoomMax, pct));
    out.cx = (std::max)(1L, static_cast<LONG>(static_cast<long long>(s->canvas.cx) * pct / 100));
    out.cy = (std::max)(1L, static_cast<LONG>(static_cast<long long>(s->canvas.cy) * pct / 100));
  }
  return out;
}

bool Scrollable(const ViewerState* s, const RECT& client, SIZE* dispOut = nullptr) {
  const SIZE d = ComputeDisplayed(s, client);
  if (dispOut) *dispOut = d;
  return d.cx > (client.right - client.left) || d.cy > (client.bottom - client.top);
}

void ClampOffset(ViewerState* s, const RECT& client, const SIZE& disp) {
  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;
  const LONG maxx = (std::max)(0, static_cast<int>(disp.cx) - cw);
  const LONG maxy = (std::max)(0, static_cast<int>(disp.cy) - ch);
  s->offset.cx = (std::max)(0L, (std::min)(maxx, s->offset.cx));
  s->offset.cy = (std::max)(0L, (std::min)(maxy, s->offset.cy));
}

// Sync the H/V scrollbars to the current zoom. `GetClientRect` does not reflect
// a bar's appearance synchronously, so instead of looping on it we RESERVE the
// scrollbar thickness ourselves: derive the no-bars content box, decide which
// bars are needed (two passes for the mutual dependency), show/hide them
// explicitly, and set the page size from the reserved content box. Finally force
// a full frame+client redraw to clear stale scrollbar/strip artifacts.
void UpdateScrollbars(ViewerState* s) {
  if (s->updating_scrollbars) return;
  s->updating_scrollbars = true;

  const int sxV = GetSystemMetrics(SM_CXVSCROLL);
  const int syH = GetSystemMetrics(SM_CYHSCROLL);

  RECT c;
  GetClientRect(s->hwnd, &c);
  // Content box as if no bars were shown (add back any currently shown).
  const int fullW = (c.right - c.left) + (s->vbar ? sxV : 0);
  const int fullH = (c.bottom - c.top) + (s->hbar ? syH : 0);

  RECT fullRc{0, 0, fullW, fullH};
  const SIZE disp = ComputeDisplayed(s, fullRc);  // fit mode fits this box -> no bars

  bool needH = false, needV = false;
  for (int i = 0; i < 2; ++i) {
    needH = disp.cx > (fullW - (needV ? sxV : 0));
    needV = disp.cy > (fullH - (needH ? syH : 0));
  }
  const int contentW = fullW - (needV ? sxV : 0);
  const int contentH = fullH - (needH ? syH : 0);

  if (needH != s->hbar) { ShowScrollBar(s->hwnd, SB_HORZ, needH); s->hbar = needH; }
  if (needV != s->vbar) { ShowScrollBar(s->hwnd, SB_VERT, needV); s->vbar = needV; }

  s->offset.cx = (std::max)(0L, (std::min)(static_cast<LONG>((std::max)(0, static_cast<int>(disp.cx) - contentW)), s->offset.cx));
  s->offset.cy = (std::max)(0L, (std::min)(static_cast<LONG>((std::max)(0, static_cast<int>(disp.cy) - contentH)), s->offset.cy));

  SCROLLINFO si{};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  if (needH) {
    si.nMax = disp.cx - 1; si.nPage = static_cast<UINT>(contentW); si.nPos = s->offset.cx;
    SetScrollInfo(s->hwnd, SB_HORZ, &si, TRUE);
  }
  if (needV) {
    si.nMax = disp.cy - 1; si.nPage = static_cast<UINT>(contentH); si.nPos = s->offset.cy;
    SetScrollInfo(s->hwnd, SB_VERT, &si, TRUE);
  }

  s->updating_scrollbars = false;
  RedrawWindow(s->hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
}

void FillBitmapInfoTopDown(BITMAPINFO* bi, int w, int h) {
  ZeroMemory(bi, sizeof(BITMAPINFO));
  bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi->bmiHeader.biWidth = w;
  bi->bmiHeader.biHeight = -h;  // top-down
  bi->bmiHeader.biPlanes = 1;
  bi->bmiHeader.biBitCount = 32;
  bi->bmiHeader.biCompression = BI_RGB;
}

// Tell Opus the current displayed size (drives its zoom readout / framing).
void SendSizeChange(ViewerState* s, const RECT& client) {
  DVPNMSIZECHANGE n{};
  n.hdr.hwndFrom = s->hwnd;
  n.hdr.idFrom = 0;
  n.hdr.code = DVPN_SIZECHANGE;
  n.szSize = ComputeDisplayed(s, client);
  SendMessageW(GetParent(s->hwnd), WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&n));
}

// ---- Auto-hiding playback bar ----------------------------------------------

struct BarLayout {
  bool valid = false;
  RECT bar{}, prev{}, play{}, next{}, track{}, label{};
};

BarLayout ComputeBar(const ViewerState* s, const RECT& client) {
  BarLayout b;
  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;
  if (s->frame_count <= 1 || cw < 220 || ch < kBarHeight + 16) return b;
  b.valid = true;
  const int by = ch - kBarHeight;
  b.bar = {0, by, cw, ch};
  int x = 8;
  auto btn = [&](RECT& r) { r = {x, by, x + kBtnW, ch}; x += kBtnW; };
  btn(b.prev);
  btn(b.play);
  btn(b.next);
  x += 10;
  const int labelW = 86;
  b.label = {cw - labelW - 10, by, cw - 10, ch};
  b.track = {x, by, b.label.left - 14, ch};
  return b;
}

void FillPoly(HDC hdc, const POINT* pts, int n, COLORREF c) {
  HBRUSH br = CreateSolidBrush(c);
  HGDIOBJ ob = SelectObject(hdc, br);
  HGDIOBJ op = SelectObject(hdc, GetStockObject(NULL_PEN));
  Polygon(hdc, pts, n);
  SelectObject(hdc, op);
  SelectObject(hdc, ob);
  DeleteObject(br);
}

void FillRC(HDC hdc, int l, int t, int r, int b, COLORREF c) {
  RECT rc{l, t, r, b};
  HBRUSH br = CreateSolidBrush(c);
  FillRect(hdc, &rc, br);
  DeleteObject(br);
}

// A translucent solid fill (AlphaBlend a 1x1 colour at constant alpha).
void AlphaFillRect(HDC hdc, const RECT& r, COLORREF color, BYTE alpha) {
  HDC mem = CreateCompatibleDC(hdc);
  if (!mem) return;
  BITMAPINFO bi;
  FillBitmapInfoTopDown(&bi, 1, 1);
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp && bits) {
    auto* p = static_cast<uint8_t*>(bits);
    p[0] = GetBValue(color); p[1] = GetGValue(color); p[2] = GetRValue(color); p[3] = 255;
    HGDIOBJ old = SelectObject(mem, bmp);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, 0};
    AlphaBlend(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top, mem, 0, 0, 1, 1, bf);
    SelectObject(mem, old);
  }
  if (bmp) DeleteObject(bmp);
  DeleteDC(mem);
}

void DrawBar(ViewerState* s, HDC hdc, const BarLayout& b) {
  const COLORREF fg = RGB(235, 235, 238);
  const COLORREF accent = RGB(90, 170, 255);
  const int g = 7;  // glyph half-size

  AlphaFillRect(hdc, b.bar, RGB(18, 18, 22), 190);

  // Previous frame: |<
  {
    const int cx = (b.prev.left + b.prev.right) / 2, cy = (b.prev.top + b.prev.bottom) / 2;
    FillRC(hdc, cx - g - 4, cy - g, cx - g - 1, cy + g, fg);
    POINT p[3] = {{cx + g, cy - g}, {cx + g, cy + g}, {cx - g, cy}};
    FillPoly(hdc, p, 3, fg);
  }
  // Play (>) or Pause (||)
  {
    const int cx = (b.play.left + b.play.right) / 2, cy = (b.play.top + b.play.bottom) / 2;
    if (s->paused) {
      POINT p[3] = {{cx - g, cy - g}, {cx - g, cy + g}, {cx + g, cy}};
      FillPoly(hdc, p, 3, fg);
    } else {
      FillRC(hdc, cx - g, cy - g, cx - 2, cy + g, fg);
      FillRC(hdc, cx + 2, cy - g, cx + g, cy + g, fg);
    }
  }
  // Next frame: >|
  {
    const int cx = (b.next.left + b.next.right) / 2, cy = (b.next.top + b.next.bottom) / 2;
    POINT p[3] = {{cx - g, cy - g}, {cx - g, cy + g}, {cx + g, cy}};
    FillPoly(hdc, p, 3, fg);
    FillRC(hdc, cx + g + 1, cy - g, cx + g + 4, cy + g, fg);
  }

  // Progress track + fill + thumb.
  const int ty = (b.track.top + b.track.bottom) / 2;
  const int tw = b.track.right - b.track.left;
  const int fc = static_cast<int>((std::max)(static_cast<size_t>(1), s->frame_count));
  const double frac = (fc > 1 && s->cur_index >= 0) ? static_cast<double>(s->cur_index) / (fc - 1) : 0.0;
  const int thumbX =
      s->scrubbing
          ? (std::max)(static_cast<int>(b.track.left),
                       (std::min)(static_cast<int>(b.track.right), s->scrub_x))
          : b.track.left + static_cast<int>(frac * tw + 0.5);
  for (int dy = -1; dy <= 1; ++dy) {  // 3px line via rects (no pen juggling)
    FillRC(hdc, b.track.left, ty + dy, b.track.right, ty + dy + 1, RGB(90, 90, 96));
    FillRC(hdc, b.track.left, ty + dy, thumbX, ty + dy + 1, accent);
  }
  {
    HBRUSH br = CreateSolidBrush(fg);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, thumbX - 6, ty - 6, thumbX + 6, ty + 6);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(br);
  }

  // Frame counter "N / M".
  wchar_t txt[32];
  wsprintfW(txt, L"%d / %d", s->cur_index + 1, static_cast<int>(s->frame_count));
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, fg);
  RECT lab = b.label;
  DrawTextW(hdc, txt, -1, &lab, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void InvalidateBar(ViewerState* s) {
  RECT client;
  GetClientRect(s->hwnd, &client);
  const BarLayout b = ComputeBar(s, client);
  RECT r = b.valid ? b.bar : client;
  InvalidateRect(s->hwnd, &r, FALSE);
}

void ShowBar(ViewerState* s) {
  if (s->frame_count <= 1) return;
  const bool was = s->bar_visible;
  s->bar_visible = true;
  SetTimer(s->hwnd, kHideTimerId, kHideDelayMs, nullptr);  // (re)start idle timer
  if (!was) InvalidateBar(s);
}

bool CursorOverBar(ViewerState* s) {
  POINT p;
  if (!GetCursorPos(&p)) return false;
  ScreenToClient(s->hwnd, &p);
  RECT client;
  GetClientRect(s->hwnd, &client);
  const BarLayout b = ComputeBar(s, client);
  return b.valid && PtInRect(&b.bar, p);
}

void HideBar(ViewerState* s) {
  // Don't hide while the pointer is resting on the bar (so buttons stay aimable).
  if (s->scrubbing || CursorOverBar(s)) {
    SetTimer(s->hwnd, kHideTimerId, kHideDelayMs, nullptr);
    return;
  }
  KillTimer(s->hwnd, kHideTimerId);
  if (s->bar_visible) {
    s->bar_visible = false;
    InvalidateBar(s);
  }
}

void SeekBarToX(ViewerState* s, int x, const RECT& track) {
  const int fc = static_cast<int>((std::max)(static_cast<size_t>(1), s->frame_count));
  const int w = static_cast<int>(track.right - track.left);
  if (w <= 0 || !s->source) return;
  int rel = x - static_cast<int>(track.left);
  rel = (std::max)(0, (std::min)(w, rel));
  int idx = (fc > 1) ? (rel * (fc - 1) + w / 2) / w : 0;
  idx = (std::max)(0, (std::min)(fc - 1, idx));
  if (idx == s->cur_index) return;
  Frame f;
  if (s->source->SeekToFrame(static_cast<size_t>(idx), f)) {
    s->cur_index = idx;
    ShowFrame(s, f);
  }
}

void PaintViewer(ViewerState* s, HDC hdc, const RECT& client) {
  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;

  // Double buffer into a client-sized top-down DIBSection (no DDB).
  BITMAPINFO bi;
  FillBitmapInfoTopDown(&bi, cw, (std::max)(1, ch));
  void* bits = nullptr;
  HDC mem = CreateCompatibleDC(hdc);
  HBITMAP back = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ oldBack = back ? SelectObject(mem, back) : nullptr;

  HDC target = (mem && back) ? mem : hdc;  // fall back to direct paint

  HBRUSH bgBrush = CreateSolidBrush(s->bg);
  RECT fillRc{0, 0, cw, ch};
  FillRect(target, &fillRc, bgBrush);
  DeleteObject(bgBrush);

  if (s->has_image && !s->composited.empty()) {
    const SIZE disp = ComputeDisplayed(s, client);
    // Centre when the image fits; scroll by the offset when it's larger.
    const int x = (disp.cx <= cw) ? (cw - disp.cx) / 2 : -s->offset.cx;
    const int y = (disp.cy <= ch) ? (ch - disp.cy) / 2 : -s->offset.cy;

    BITMAPINFO src;
    FillBitmapInfoTopDown(&src, s->canvas.cx, s->canvas.cy);
    SetStretchBltMode(target, HALFTONE);
    SetBrushOrgEx(target, 0, 0, nullptr);
    StretchDIBits(target, x, y, disp.cx, disp.cy, 0, 0, s->canvas.cx, s->canvas.cy,
                  s->composited.data(), &src, DIB_RGB_COLORS, SRCCOPY);
  }

  if (s->bar_visible) {
    const BarLayout bl = ComputeBar(s, client);
    if (bl.valid) DrawBar(s, target, bl);
  }

  if (target == mem) {
    BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
  }

  if (oldBack) SelectObject(mem, oldBack);
  if (back) DeleteObject(back);
  if (mem) DeleteDC(mem);
}

void LoadFromBytes(ViewerState* s, const uint8_t* data, size_t len) {
  KillTimer(s->hwnd, kTimerId);
  KillTimer(s->hwnd, kHideTimerId);
  s->bar_visible = false;
  s->scrubbing = false;
  s->has_image = false;
  s->composited.clear();

  s->source = OpenAnimSource(data, len);
  if (len == 0 || data == nullptr || !s->source) {
    InvalidateRect(s->hwnd, nullptr, TRUE);
    return;
  }
  s->canvas.cx = s->source->CanvasWidth();
  s->canvas.cy = s->source->CanvasHeight();
  s->frame_count = s->source->FrameCount();
  s->offset = {0, 0};
  s->paused = false;
  s->cur_index = -1;
  AdvanceFrame(s);  // show first frame + arm timer if animated
  UpdateScrollbars(s);

  RECT client;
  if (GetClientRect(s->hwnd, &client)) SendSizeChange(s, client);
}

void LoadFile(ViewerState* s, LPCWSTR path) {
  const std::vector<uint8_t> bytes = ReadWholeFile(path);
  LoadFromBytes(s, bytes.data(), bytes.size());
}

void LoadStream(ViewerState* s, IStream* stream) {
  const std::vector<uint8_t> bytes = ReadWholeStream(stream);
  LoadFromBytes(s, bytes.data(), bytes.size());
}

void ClearImage(ViewerState* s) {
  KillTimer(s->hwnd, kTimerId);
  s->has_image = false;
  s->composited.clear();
  s->composited.shrink_to_fit();
  s->source.reset();  // free decoder + canvases
  InvalidateRect(s->hwnd, nullptr, TRUE);
}

// Items Opus merges into its own right-click menu, grouped under an "animview"
// submenu (playback for animations, plus zoom). Labels are kept in ViewerState so
// the pointers stay valid after we return to Opus.
DVPCONTEXTMENUITEM* BuildContextMenu(ViewerState* s, DWORD* pNum) {
  int n = 0;
  auto add = [&](const wchar_t* label, DWORD flags, UINT id) {
    s->menu_labels[n] = label ? label : L"";
    s->menu_items[n].lpszLabel = label ? const_cast<LPWSTR>(s->menu_labels[n].c_str()) : nullptr;
    s->menu_items[n].dwFlags = flags;
    s->menu_items[n].uID = id;
    ++n;
  };

  // Playback only — zoom is Opus's job (we honour its zoom mode). Nothing to add
  // for a still image.
  if (s->frame_count <= 1) {
    if (pNum) *pNum = 0;
    return nullptr;
  }
  add(L"animview", DVPCMF_BEGINSUBMENU, 0);  // submenu header
  add(s->paused ? L"Play" : L"Pause", 0, CMD_PLAYPAUSE);
  add(L"Previous frame", 0, CMD_PREV);
  add(L"Next frame", DVPCMF_ENDSUBMENU, CMD_NEXT);  // closes the submenu

  if (pNum) *pNum = static_cast<DWORD>(n);
  return s->menu_items;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_CREATE) {
    auto* s = new ViewerState();
    s->hwnd = hWnd;
    s->zoom_req = ZOOM_FITPAGE;  // default until Opus sends its zoom
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
    return 0;
  }

  ViewerState* s = GetState(hWnd);
  if (s == nullptr) return DefWindowProcW(hWnd, msg, wParam, lParam);

  switch (msg) {
    case DVPLUGINMSG_LOADW:
      LoadFile(s, reinterpret_cast<LPCWSTR>(lParam));
      return TRUE;

    case DVPLUGINMSG_LOADSTREAMW:
      // wParam = name (LPWSTR), lParam = IStream*. Opus uses this for the
      // preview pane and for files it already holds open.
      LoadStream(s, reinterpret_cast<IStream*>(lParam));
      return TRUE;

    case DVPLUGINMSG_CLEAR:
      ClearImage(s);
      return 0;

    case DVPLUGINMSG_GETCAPABILITIES:
      return VPCAPABILITY_RESIZE_FIT | VPCAPABILITY_RESIZE_ANY |
             VPCAPABILITY_ADDCONTEXTMENU;

    case DVPLUGINMSG_ADDCONTEXTMENU:
      return reinterpret_cast<LRESULT>(
          BuildContextMenu(s, reinterpret_cast<DWORD*>(wParam)));

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case CMD_PLAYPAUSE:
          if (s->paused) ResumePlayback(s); else PausePlayback(s);
          return 0;
        case CMD_PREV: StepPrev(s); return 0;
        case CMD_NEXT: StepNext(s); return 0;
        default: break;
      }
      break;

    case DVPLUGINMSG_APPCOMMAND:
      switch (GET_APPCOMMAND_LPARAM(lParam)) {
        case APPCOMMAND_MEDIA_NEXTTRACK: StepNext(s); return TRUE;
        case APPCOMMAND_MEDIA_PREVIOUSTRACK: StepPrev(s); return TRUE;
        case APPCOMMAND_MEDIA_PAUSE:
        case APPCOMMAND_MEDIA_STOP: PausePlayback(s); return TRUE;
        case APPCOMMAND_MEDIA_PLAY: ResumePlayback(s); return TRUE;
        case APPCOMMAND_MEDIA_PLAY_PAUSE:
          if (s->paused) ResumePlayback(s); else PausePlayback(s);
          return TRUE;
        default: break;
      }
      break;

    case DVPLUGINMSG_GETPICSIZE:
    case DVPLUGINMSG_GETORIGINALPICSIZE:
      if (wParam) *reinterpret_cast<LPINT>(wParam) = 32;
      if (lParam) *reinterpret_cast<LPSIZE>(lParam) = s->canvas;
      return TRUE;

    case DVPLUGINMSG_SETZOOM:
    case DVPLUGINMSG_ZOOM: {
      // Honour Opus's zoom factor verbatim (like the OG), so WebP/GIF follow the
      // same zoom mode as other image plugins. Keep the mode sentinels intact:
      // ZOOM_FITPAGE (-1) and ZOOM_ORIGINAL (0).
      s->zoom_req = static_cast<int>(wParam);
      s->offset = {0, 0};
      UpdateScrollbars(s);
      RECT client;
      if (GetClientRect(hWnd, &client)) SendSizeChange(s, client);
      InvalidateRect(hWnd, nullptr, FALSE);
      return 0;
    }

    case DVPLUGINMSG_GETZOOMFACTOR:
      // Return the RAW factor, not a computed percent — otherwise Opus loses the
      // fit/original MODE (it showed "100%" instead of "FIT" and reset each image).
      return s->zoom_req;

    case DVPLUGINMSG_GETZOOMLIMITS:
      return (static_cast<LRESULT>(kZoomMax) << 16) | kZoomMin;

    case DVPLUGINMSG_REDRAW:
      if (wParam) s->bg = static_cast<COLORREF>(lParam);  // bg colour changed
      InvalidateRect(hWnd, nullptr, TRUE);
      return 0;

    case DVPLUGINMSG_RESIZE:
      // Opus has ALREADY resized our window (we also get a WM_SIZE). Repositioning
      // ourselves here mis-placed the window relative to Opus's menu/toolbar, which
      // threw off the scrollbars. Just react, like Leo's "fallthrough to WM_SIZE".
      UpdateScrollbars(s);
      InvalidateRect(hWnd, nullptr, FALSE);
      return 0;

    case WM_TIMER:
      if (wParam == kTimerId) AdvanceFrame(s);
      else if (wParam == kHideTimerId) HideBar(s);
      return 0;

    case WM_SIZE:
      UpdateScrollbars(s);
      InvalidateRect(hWnd, nullptr, FALSE);
      return 0;

    case WM_HSCROLL:
    case WM_VSCROLL: {
      const bool horz = (msg == WM_HSCROLL);
      RECT client;
      GetClientRect(hWnd, &client);
      SIZE disp;
      Scrollable(s, client, &disp);
      const int extent = horz ? (client.right - client.left) : (client.bottom - client.top);
      const int line = (std::max)(1, extent / 10);
      const int page = (std::max)(1, extent * 9 / 10);
      LONG& pos = horz ? s->offset.cx : s->offset.cy;
      switch (LOWORD(wParam)) {
        case SB_LINELEFT: pos -= line; break;    // == SB_LINEUP
        case SB_LINERIGHT: pos += line; break;   // == SB_LINEDOWN
        case SB_PAGELEFT: pos -= page; break;    // == SB_PAGEUP
        case SB_PAGERIGHT: pos += page; break;   // == SB_PAGEDOWN
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
          SCROLLINFO si{};
          si.cbSize = sizeof(si);
          si.fMask = SIF_TRACKPOS;
          if (GetScrollInfo(hWnd, horz ? SB_HORZ : SB_VERT, &si)) pos = si.nTrackPos;
          break;
        }
        default: break;
      }
      ClampOffset(s, client, disp);
      SetScrollPos(hWnd, horz ? SB_HORZ : SB_VERT, pos, TRUE);
      InvalidateRect(hWnd, nullptr, FALSE);
      return 0;
    }

    case WM_MOUSEWHEEL: {
      RECT client;
      GetClientRect(hWnd, &client);
      SIZE disp;
      Scrollable(s, client, &disp);
      const int ch = client.bottom - client.top;
      if (disp.cy > ch) {  // scroll vertically when there's somewhere to go
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        s->offset.cy -= (delta / WHEEL_DELTA) * (std::max)(1, ch / 8);
        ClampOffset(s, client, disp);
        SetScrollPos(hWnd, SB_VERT, s->offset.cy, TRUE);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
      }
      break;  // not scrollable -> let Opus use the wheel (zoom / next image)
    }

    case WM_LBUTTONDOWN: {
      const int mx = static_cast<short>(LOWORD(lParam));
      const int my = static_cast<short>(HIWORD(lParam));
      RECT client;
      GetClientRect(hWnd, &client);

      // Playback bar first (if visible and the click lands on it).
      if (s->bar_visible) {
        const BarLayout b = ComputeBar(s, client);
        POINT pt{mx, my};
        if (b.valid && PtInRect(&b.bar, pt)) {
          if (PtInRect(&b.prev, pt)) { StepPrev(s); return 0; }
          if (PtInRect(&b.play, pt)) {
            if (s->paused) ResumePlayback(s); else PausePlayback(s);
            return 0;
          }
          if (PtInRect(&b.next, pt)) { StepNext(s); return 0; }
          if (PtInRect(&b.track, pt)) {
            s->scrubbing = true;
            s->scrub_x = mx;
            PausePlayback(s);
            SetCapture(hWnd);
            SeekBarToX(s, mx, b.track);
            return 0;
          }
          return 0;  // click on bar background: consume, don't pan
        }
      }

      if (Scrollable(s, client)) {  // only hijack the drag when there's room to pan
        s->panning = true;
        s->pan_anchor.x = mx;
        s->pan_anchor.y = my;
        s->pan_anchor_offset = s->offset;
        SetCapture(hWnd);
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
        return 0;
      }
      break;  // at fit, pass clicks through to Opus (advance etc.)
    }

    case WM_MOUSEMOVE: {
      const int mx = static_cast<short>(LOWORD(lParam));
      const int my = static_cast<short>(HIWORD(lParam));
      if (!s->mouse_tracking) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
        TrackMouseEvent(&tme);
        s->mouse_tracking = true;
      }
      if (s->scrubbing) {
        if (!(wParam & MK_LBUTTON)) {  // button released outside our notice
          s->scrubbing = false;
          ReleaseCapture();
          return 0;
        }
        s->scrub_x = mx;
        RECT client;
        GetClientRect(hWnd, &client);
        const BarLayout b = ComputeBar(s, client);
        if (b.valid) {
          SeekBarToX(s, mx, b.track);
          InvalidateRect(hWnd, &b.bar, FALSE);  // move the thumb even within a frame
        }
        return 0;
      }
      if (s->panning) {
        RECT client;
        GetClientRect(hWnd, &client);
        SIZE disp;
        Scrollable(s, client, &disp);
        s->offset.cx = s->pan_anchor_offset.cx - (mx - s->pan_anchor.x);
        s->offset.cy = s->pan_anchor_offset.cy - (my - s->pan_anchor.y);
        ClampOffset(s, client, disp);
        SetScrollPos(hWnd, SB_HORZ, s->offset.cx, TRUE);
        SetScrollPos(hWnd, SB_VERT, s->offset.cy, TRUE);
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
      }
      ShowBar(s);  // reveal the playback bar + reset its idle timer
      break;
    }

    case WM_MOUSELEAVE:
      s->mouse_tracking = false;
      HideBar(s);
      return 0;

    case WM_LBUTTONUP:
      if (s->scrubbing) {
        s->scrubbing = false;
        ReleaseCapture();
        return 0;
      }
      if (s->panning) {
        s->panning = false;
        ReleaseCapture();
        return 0;
      }
      break;

    case WM_CAPTURECHANGED:
      s->panning = false;
      s->scrubbing = false;
      return 0;

    case WM_ERASEBKGND:
      return 1;  // painted in WM_PAINT

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      RECT client;
      GetClientRect(hWnd, &client);
      PaintViewer(s, hdc, client);
      EndPaint(hWnd, &ps);
      return 0;
    }

    case WM_NCDESTROY:
      KillTimer(hWnd, kTimerId);
      delete s;
      SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
      return 0;

    default:
      break;
  }

  // Unhandled plugin messages must return 0; real window messages go to DefWindowProc.
  if (msg >= DVPLUGINMSG_BASE) return 0;
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

HINSTANCE g_inst = nullptr;

}  // namespace

bool RegisterViewerClass(HINSTANCE hInstance) {
  g_inst = hInstance;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;  // we paint everything
  wc.lpszClassName = kClassName;
  return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void UnregisterViewerClass(HINSTANCE hInstance) {
  UnregisterClassW(kClassName, hInstance);
}

HWND CreateViewerWindow(HWND hParent, const RECT* rc, DWORD /*dwFlags*/) {
  const int x = rc ? rc->left : 0;
  const int y = rc ? rc->top : 0;
  const int w = rc ? (rc->right - rc->left) : 0;
  const int h = rc ? (rc->bottom - rc->top) : 0;
  return CreateWindowExW(
      0, kClassName, L"",
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_HSCROLL | WS_VSCROLL,
      x, y, w, h, hParent, nullptr, g_inst, nullptr);
}

}  // namespace animview
