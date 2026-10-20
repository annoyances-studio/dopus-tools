// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
#include <windows.h>

namespace animview {

// Register/unregister the viewer window class (once per plugin load/unload).
bool RegisterViewerClass(HINSTANCE hInstance);
void UnregisterViewerClass(HINSTANCE hInstance);

// Create the owned viewer child window Opus hosts (DVP_CreateViewer).
HWND CreateViewerWindow(HWND hParent, const RECT* rc, DWORD dwFlags);

}  // namespace animview
