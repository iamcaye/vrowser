// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/ui/status_line.hpp"

#include <string>

namespace vrowser {
namespace {

constexpr std::string_view kEllipsis = "...";

// Cuts `text` to `max_chars`, marking the cut. Keeps at least `min_keep`
// characters even when that is longer than `max_chars`.
std::string CutEnd(std::string_view text, size_t max_chars, size_t min_keep) {
  if (text.size() <= max_chars) {
    return std::string(text);
  }
  size_t keep = max_chars > kEllipsis.size() ? max_chars - kEllipsis.size() : 0;
  if (keep < min_keep) {
    keep = min_keep;
  }
  if (keep >= text.size()) {
    return std::string(text);
  }
  return std::string(text.substr(0, keep)) + std::string(kEllipsis);
}

}  // namespace

std::string ModeLabel(Mode mode) {
  switch (mode) {
    case Mode::kNormal:
      return "NORMAL";
    case Mode::kInsert:
      return "INSERT";
    case Mode::kCommand:
      return "COMMAND";
    case Mode::kHint:
      return "HINT";
  }
  return std::string();
}

std::string PendingKeys(const KeySequenceState& pending) {
  std::string text = pending.count > 0 ? std::to_string(pending.count) : "";
  for (const std::string& key : pending.keys) {
    text += key;
  }
  return text;
}

std::string ShortenUrl(std::string_view url, size_t max_chars) {
  const size_t scheme_end = url.find("://");
  if (scheme_end == std::string_view::npos) {
    // "data:", "about:", and similar: keep at least the scheme.
    const size_t colon = url.find(':');
    const size_t scheme = colon == std::string_view::npos ? 0 : colon + 1;
    return CutEnd(url, max_chars, scheme);
  }
  const size_t authority_start = scheme_end + 3;
  size_t authority_end = url.find_first_of("/?#", authority_start);
  if (authority_end == std::string_view::npos) {
    authority_end = url.size();
  }
  std::string display(url.substr(0, authority_start));
  std::string_view authority =
      url.substr(authority_start, authority_end - authority_start);
  const size_t at = authority.rfind('@');
  if (at != std::string_view::npos) {
    authority.remove_prefix(at + 1);
  }
  display += authority;
  const size_t origin_size = display.size();
  display += url.substr(authority_end);
  return CutEnd(display, max_chars, origin_size);
}

std::string FormatStatusLine(const StatusLineInput& input, size_t max_chars) {
  std::string prefix = ModeLabel(input.mode) + "  ";
  const std::string pending = PendingKeys(input.pending);
  if (!pending.empty()) {
    prefix += pending + "  ";
  }
  const std::string suffix = input.loading ? "  loading" : "";
  const size_t fixed = prefix.size() + suffix.size();
  const size_t url_budget = max_chars > fixed ? max_chars - fixed : 0;
  return prefix + ShortenUrl(input.url, url_budget) + suffix;
}

}  // namespace vrowser
