// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

namespace vrowser {

// Search engine used until there is a setting. The query is appended
// form-encoded.
inline constexpr std::string_view kDefaultSearchUrlPrefix =
    "https://duckduckgo.com/?q=";

enum class UrlInputKind {
  kEmpty,     // Only whitespace was typed.
  kUrl,       // `url` is the address to load.
  kSearch,    // `url` is a search for the typed text.
  kRejected,  // A scheme the command line must not load; see `error`.
};

struct UrlInput {
  UrlInputKind kind{UrlInputKind::kEmpty};
  // Set for kUrl and kSearch.
  std::string url;
  // Set for kRejected.
  std::string error;
};

// Turns what the user typed on the command line into something to load. The
// text is untrusted:
//   - `http://` and `https://` URLs are used as typed, and so is
//     `about:blank`;
//   - a host (`example.com`, `localhost:8080`, `127.0.0.1`, `[::1]`),
//     optionally followed by a path, gets `https://`;
//   - any other scheme (`javascript:`, `data:`, `file:`, `chrome://`, ...) is
//     rejected;
//   - anything else, including text with spaces, becomes a search.
UrlInput ClassifyUrlInput(std::string_view text);

// The default engine's search URL for `query`.
std::string SearchUrl(std::string_view query);

}  // namespace vrowser
