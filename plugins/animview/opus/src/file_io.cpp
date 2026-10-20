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

std::vector<uint8_t> ReadWholeStream(IStream* stream) {
  std::vector<uint8_t> out;
  if (stream == nullptr) return out;

  LARGE_INTEGER zero{};
  stream->Seek(zero, STREAM_SEEK_SET, nullptr);  // start from the beginning

  constexpr size_t kChunk = 64 * 1024;
  constexpr size_t kCap = 256ull * 1024 * 1024;  // 256 MB
  size_t total = 0;
  for (;;) {
    out.resize(total + kChunk);
    ULONG read = 0;
    const HRESULT hr = stream->Read(out.data() + total, static_cast<ULONG>(kChunk), &read);
    total += read;
    if (total > kCap) {
      out.clear();
      return out;
    }
    if (read == 0 || (hr != S_OK && hr != S_FALSE)) break;
  }
  out.resize(total);
  return out;
}

}  // namespace animview
