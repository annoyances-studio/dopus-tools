// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "file_io.h"

namespace animview {

std::vector<uint8_t> ReadWholeFile(LPCWSTR path) {
  std::vector<uint8_t> bytes;
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return bytes;

  LARGE_INTEGER size{};
  constexpr long long kMaxBytes = 256LL * 1024 * 1024;  // 256 MB cap
  if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart <= kMaxBytes) {
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
        read != bytes.size()) {
      bytes.clear();
    }
  }
  CloseHandle(h);
  return bytes;
}

}  // namespace animview
