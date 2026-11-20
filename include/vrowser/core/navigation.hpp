// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace vrowser {

// Holds the URL most recently requested for the open window. Commands and the
// relaunch path call Request(); the CEF adapter listens and performs the load
// on the UI thread. Not thread-safe: use it from one thread (the CEF UI
// thread in the app).
class NavigationController {
 public:
  using Listener = std::function<void(const std::string& url)>;

  // Trims surrounding whitespace and records the URL. Every accepted request
  // counts as a change, so requesting the same URL again reloads it. Returns
  // false for an empty URL or a `javascript:` URL, which would run script in
  // the current page.
  bool Request(std::string_view url);

  // Empty until the first accepted request.
  const std::string& requested_url() const { return requested_url_; }
  // Increments on every accepted request. 0 means none yet.
  uint64_t sequence() const { return sequence_; }

  // Called after each accepted request. Replaces any previous listener.
  void SetListener(Listener listener) { listener_ = std::move(listener); }

 private:
  std::string requested_url_;
  uint64_t sequence_{0};
  Listener listener_;
};

}  // namespace vrowser
