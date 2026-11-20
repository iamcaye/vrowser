// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/tab_list.hpp"

#include <algorithm>
#include <cstddef>

namespace vrowser {

bool TabList::Contains(TabId id) const {
  return std::find(tabs_.begin(), tabs_.end(), id) != tabs_.end();
}

bool TabList::Append(TabId id) {
  if (Contains(id)) {
    return false;
  }
  tabs_.push_back(id);
  active_ = id;
  return true;
}

bool TabList::Close(TabId id) {
  const auto it = std::find(tabs_.begin(), tabs_.end(), id);
  if (it == tabs_.end()) {
    return false;
  }
  const auto index = it - tabs_.begin();
  tabs_.erase(it);
  if (active_ != id) {
    return true;
  }
  if (tabs_.empty()) {
    active_.reset();
  } else {
    const auto next = std::min<std::ptrdiff_t>(
        index, static_cast<std::ptrdiff_t>(tabs_.size()) - 1);
    active_ = tabs_[static_cast<size_t>(next)];
  }
  return true;
}

bool TabList::Activate(TabId id) {
  if (!Contains(id)) {
    return false;
  }
  active_ = id;
  return true;
}

}  // namespace vrowser
