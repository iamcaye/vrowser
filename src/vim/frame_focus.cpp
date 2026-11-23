// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/frame_focus.hpp"

#include <algorithm>

namespace vrowser {

void FrameFocus::Report(std::string_view frame, bool editable) {
  const auto it = std::find_if(
      reports_.begin(), reports_.end(),
      [frame](const auto& report) { return report.first == frame; });
  if (it != reports_.end()) {
    reports_.erase(it);
  }
  if (reports_.size() >= kMaxFrames) {
    reports_.erase(reports_.begin());
  }
  reports_.emplace_back(std::string(frame), editable);
}

bool FrameFocus::Editable(std::string_view focused_frame) const {
  if (focused_frame.empty()) {
    return !reports_.empty() && reports_.back().second;
  }
  const auto it = std::find_if(reports_.begin(), reports_.end(),
                               [focused_frame](const auto& report) {
                                 return report.first == focused_frame;
                               });
  return it != reports_.end() && it->second;
}

}  // namespace vrowser
