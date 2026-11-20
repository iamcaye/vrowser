// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace vrowser {

// CEF browser identifier. The core never sees the browser object itself.
using TabId = int;

// Ordered tabs of one window plus the active tab. The CEF client keeps one per
// main window and shows only the active browser.
class TabList {
 public:
  bool empty() const { return tabs_.empty(); }
  size_t size() const { return tabs_.size(); }
  const std::vector<TabId>& tabs() const { return tabs_; }
  std::optional<TabId> active() const { return active_; }
  bool Contains(TabId id) const;

  // Appends `id` at the end and makes it active. Returns false for a
  // duplicate.
  bool Append(TabId id);

  // Removes `id`. When it was active, the tab that took its place becomes
  // active, or the new last tab when it was last. Returns false when absent.
  bool Close(TabId id);

  // Makes `id` active. Returns false when absent.
  bool Activate(TabId id);

 private:
  std::vector<TabId> tabs_;
  std::optional<TabId> active_;
};

}  // namespace vrowser
