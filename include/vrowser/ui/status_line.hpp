// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {

// What the status bar shows for the active tab. `url` must come from the
// browser process (the committed main-frame URL), never from page script or
// the page title.
struct StatusLineInput {
  std::string url;
  Mode mode{Mode::kNormal};
  bool loading{false};
  KeySequenceState pending;
};

// "NORMAL", "INSERT", "COMMAND", or "HINT".
std::string ModeLabel(Mode mode);

// A pending count followed by the keys typed so far ("3g"). Empty when
// nothing is pending.
std::string PendingKeys(const KeySequenceState& pending);

// Shortens `url` to `max_chars` characters for display.
//
// For a URL with an authority ("https://host/path"), the scheme and host,
// including any port, are always kept, even past `max_chars`; only the path,
// query, and fragment are cut, from the end, and marked with "...". A
// user-info prefix ("user:pass@") is dropped, so
// "https://accounts.example@evil.test/" shows as "https://evil.test/".
// Other URLs ("data:", "about:") are cut from the end.
//
// The host is shown as given. Callers pass the browser's canonical URL, which
// spells an internationalized host in punycode, so a look-alike Unicode host
// is not shown as a familiar name.
std::string ShortenUrl(std::string_view url, size_t max_chars);

// One status line: mode, pending keys, the shortened URL, and "loading" while
// the page loads, fitted to `max_chars` where the host allows it.
std::string FormatStatusLine(const StatusLineInput& input, size_t max_chars);

}  // namespace vrowser
