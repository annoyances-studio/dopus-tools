// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "animview/anim_player.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <webp/decode.h>  // MODE_BGRA
#include <webp/demux.h>   // WebPAnimDecoder

namespace animview {

struct AnimPlayer::Impl {
  std::vector<uint8_t> data;            // kept alive for the decoder
  WebPAnimDecoder* dec = nullptr;
  WebPAnimInfo info{};
  int prev_timestamp = 0;               // end-time of the previously returned frame
  bool valid = false;

  ~Impl() {
    if (dec != nullptr) WebPAnimDecoderDelete(dec);
  }
};

AnimPlayer::AnimPlayer() : p_(std::make_unique<Impl>()) {}
AnimPlayer::~AnimPlayer() = default;

bool AnimPlayer::Open(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) return false;

  p_->data.assign(data, data + len);

  WebPData webp_data;
  webp_data.bytes = p_->data.data();
  webp_data.size = p_->data.size();

  WebPAnimDecoderOptions opts;
  if (!WebPAnimDecoderOptionsInit(&opts)) return false;
  opts.color_mode = MODE_BGRA;
  opts.use_threads = 0;

  p_->dec = WebPAnimDecoderNew(&webp_data, &opts);
  if (p_->dec == nullptr) return false;

  if (!WebPAnimDecoderGetInfo(p_->dec, &p_->info)) {
    WebPAnimDecoderDelete(p_->dec);
    p_->dec = nullptr;
    return false;
  }

  p_->prev_timestamp = 0;
  p_->valid = true;
  return true;
}

bool AnimPlayer::Valid() const { return p_->valid; }
int AnimPlayer::CanvasWidth() const { return static_cast<int>(p_->info.canvas_width); }
int AnimPlayer::CanvasHeight() const { return static_cast<int>(p_->info.canvas_height); }
size_t AnimPlayer::FrameCount() const { return p_->info.frame_count; }
uint32_t AnimPlayer::LoopCount() const { return p_->info.loop_count; }

void AnimPlayer::Rewind() {
  if (p_->dec != nullptr) {
    WebPAnimDecoderReset(p_->dec);
    p_->prev_timestamp = 0;
  }
}

bool AnimPlayer::NextFrame(Frame& out) {
  if (!p_->valid || p_->dec == nullptr) return false;

  if (!WebPAnimDecoderHasMoreFrames(p_->dec)) {
    WebPAnimDecoderReset(p_->dec);  // loop
    p_->prev_timestamp = 0;
  }

  uint8_t* buf = nullptr;
  int timestamp_ms = 0;
  if (!WebPAnimDecoderGetNext(p_->dec, &buf, &timestamp_ms) || buf == nullptr) {
    return false;
  }

  out.width = static_cast<int>(p_->info.canvas_width);
  out.height = static_cast<int>(p_->info.canvas_height);
  out.duration_ms = std::max(0, timestamp_ms - p_->prev_timestamp);
  p_->prev_timestamp = timestamp_ms;

  const size_t bytes = static_cast<size_t>(out.width) * out.height * 4;
  out.bgra.resize(bytes);
  std::memcpy(out.bgra.data(), buf, bytes);
  return true;
}

}  // namespace animview
