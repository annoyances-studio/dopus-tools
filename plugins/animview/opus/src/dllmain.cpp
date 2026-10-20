// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <windows.h>

HINSTANCE g_hInstance = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      g_hInstance = hInstance;
      DisableThreadLibraryCalls(hInstance);  // no per-thread state yet
      break;
    default:
      break;
  }
  return TRUE;
}
