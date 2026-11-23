// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vrowser {

// Editable focus in one tab, as each of its frames last reported it.
//
// Each frame reports its own focused node from its renderer. Frames in
// different processes report in any order, and a frame that loses focus to
// another frame may never report a blur. So the tab is editable only when the
// frame that has focus, according to the browser process, last reported an
// editable node. Frame identifiers are opaque strings from the browser
// process, never from page content.
class FrameFocus {
 public:
  // Frames remembered at once. The oldest report is dropped beyond this, so
  // a page that creates many frames cannot grow the table without bound.
  static constexpr size_t kMaxFrames = 64;

  void Report(std::string_view frame, bool editable);
  // After a main-frame navigation every old frame is gone.
  void Clear() { reports_.clear(); }

  // `focused_frame` is the frame that has focus. When it is empty (unknown),
  // the most recent report decides.
  bool Editable(std::string_view focused_frame) const;

  size_t size() const { return reports_.size(); }

 private:
  // Oldest report first.
  std::vector<std::pair<std::string, bool>> reports_;
};

}  // namespace vrowser
