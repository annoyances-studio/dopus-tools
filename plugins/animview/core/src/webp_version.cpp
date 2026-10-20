// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/webp_version.h"

#include <webp/decode.h>  // WebPGetDecoderVersion

namespace animview {

std::string WebpLibVersion() {
  // Packed as (major << 16) | (minor << 8) | revision.
  const int v = WebPGetDecoderVersion();
  return std::to_string((v >> 16) & 0xff) + "." +
         std::to_string((v >> 8) & 0xff) + "." +
         std::to_string(v & 0xff);
}

}  // namespace animview
