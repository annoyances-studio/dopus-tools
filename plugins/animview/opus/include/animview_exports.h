// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
//
// The Directory Opus viewer-plugin entry points this DLL exports. Under a
// UNICODE build the SDK macros rewrite the unsuffixed names to their W form
// (e.g. DVP_Identify -> DVP_IdentifyW), which is exactly what Opus looks up.
//
#include <windows.h>
#include <objidl.h>  // IStream / LPSTREAM (WIN32_LEAN_AND_MEAN omits the COM headers)

#include "viewer plugins.h"

extern "C" {

__declspec(dllexport) BOOL DVP_InitEx(LPDVPINITEXDATA pInitExData);
__declspec(dllexport) void DVP_Uninit(void);
__declspec(dllexport) BOOL DVP_USBSafe(LPOPUSUSBSAFEDATA pUSBSafeData);

__declspec(dllexport) BOOL DVP_Identify(LPVIEWERPLUGININFO lpVPInfo);
__declspec(dllexport) BOOL DVP_IdentifyFileBytes(HWND hWnd, LPTSTR lpszName,
                                                 LPBYTE lpData, UINT uiDataSize,
                                                 LPVIEWERPLUGINFILEINFO lpVPFileInfo,
                                                 DWORD dwStreamFlags);

// Still path (increment #2: decode + return the first frame as a bitmap).
__declspec(dllexport) HBITMAP DVP_LoadBitmap(HWND hWnd, LPTSTR lpszName,
                                             LPVIEWERPLUGINFILEINFO lpVPFileInfo,
                                             LPSIZE lpszDesiredSize, HANDLE hAbortEvent);

// Animated path (increment #3: the owned window + playback loop).
__declspec(dllexport) HWND DVP_CreateViewer(HWND hWnd, LPRECT lpRc, DWORD dwFlags);

}  // extern "C"
