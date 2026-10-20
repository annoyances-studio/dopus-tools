// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once
#include <string>

namespace animview {

// libwebp decoder version as "major.minor.revision". Exists mainly to prove the
// libwebp dependency is fetched, built and linked (increment #1 build-spine check).
std::string WebpLibVersion();

}  // namespace animview
