// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/url_input.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace vrowser {
namespace {

// Schemes typed text must never load: script, local data, local files, and
// browser-internal pages. Any other scheme followed by `//` is rejected too.
constexpr std::string_view kRejectedSchemes[] = {
    "about",      "blob",       "data",        "devtools", "file",
    "filesystem", "javascript", "view-source", "vbscript",
};

bool IsSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

bool IsDigit(char c) {
  return c >= '0' && c <= '9';
}

bool IsAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsHexDigit(char c) {
  return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

char ToLower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(),
                    [](char x, char y) { return ToLower(x) == ToLower(y); });
}

bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && IsSpace(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && IsSpace(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

bool AllDigits(std::string_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), IsDigit);
}

// A decimal number of at most `max_digits` digits, no larger than `max`.
bool IsNumberUpTo(std::string_view text, size_t max_digits, int max) {
  if (!AllDigits(text) || text.size() > max_digits) {
    return false;
  }
  int value = 0;
  for (const char c : text) {
    value = value * 10 + (c - '0');
  }
  return value <= max;
}

bool IsPort(std::string_view text) {
  return IsNumberUpTo(text, 5, 65535);
}

bool IsIpv4(std::string_view host) {
  int parts = 0;
  while (true) {
    const size_t dot = host.find('.');
    if (!IsNumberUpTo(host.substr(0, dot), 3, 255)) {
      return false;
    }
    ++parts;
    if (dot == std::string_view::npos) {
      return parts == 4;
    }
    host.remove_prefix(dot + 1);
  }
}

// Letters, digits, and inner hyphens. Bytes above ASCII are allowed so that
// internationalized names (`münchen.de`) count as hosts.
bool IsDomainLabel(std::string_view label) {
  if (label.empty() || label.size() > 63 || label.front() == '-' ||
      label.back() == '-') {
    return false;
  }
  return std::all_of(label.begin(), label.end(), [](char c) {
    return IsAlpha(c) || IsDigit(c) || c == '-' ||
           static_cast<unsigned char>(c) >= 0x80;
  });
}

// `example.com`, `localhost`, `127.0.0.1`. A single label other than
// `localhost` is a host only with a port (`devbox:3000`).
bool IsHostName(std::string_view host, bool has_port) {
  if (EqualsIgnoreCase(host, "localhost") || IsIpv4(host)) {
    return true;
  }
  size_t labels = 0;
  std::string_view last;
  std::string_view rest = host;
  while (true) {
    const size_t dot = rest.find('.');
    last = rest.substr(0, dot);
    if (!IsDomainLabel(last)) {
      return false;
    }
    ++labels;
    if (dot == std::string_view::npos) {
      break;
    }
    rest.remove_prefix(dot + 1);
  }
  // An all-digit top-level label is a number (`1.5`) or a bad address.
  if (labels == 1) {
    return has_port && !AllDigits(last);
  }
  return !AllDigits(last);
}

// `[::1]` with the brackets.
bool IsBracketedIpv6(std::string_view host) {
  if (host.size() < 4 || host.front() != '[' || host.back() != ']') {
    return false;
  }
  const std::string_view inner = host.substr(1, host.size() - 2);
  return inner.find(':') != std::string_view::npos &&
         std::all_of(inner.begin(), inner.end(), [](char c) {
           return IsHexDigit(c) || c == ':' || c == '.';
         });
}

// True when `text` starts with a host and optional port, followed by nothing
// or by a path, query, or fragment. `user@host` is not accepted.
bool LooksLikeHost(std::string_view text) {
  const std::string_view authority = text.substr(0, text.find_first_of("/?#"));
  std::string_view host = authority;
  std::optional<std::string_view> port;
  if (!authority.empty() && authority.front() == '[') {
    const size_t close = authority.find(']');
    if (close == std::string_view::npos) {
      return false;
    }
    host = authority.substr(0, close + 1);
    const std::string_view after = authority.substr(close + 1);
    if (!after.empty()) {
      if (after.front() != ':') {
        return false;
      }
      port = after.substr(1);
    }
    return IsBracketedIpv6(host) && (!port || IsPort(*port));
  }
  const size_t colon = authority.find(':');
  if (colon != std::string_view::npos) {
    host = authority.substr(0, colon);
    port = authority.substr(colon + 1);
    if (!IsPort(*port)) {
      return false;
    }
  }
  return IsHostName(host, port.has_value());
}

// The scheme of `text` if it starts with one (`name:`), lowercased.
std::optional<std::string> SchemeOf(std::string_view text) {
  const size_t colon = text.find(':');
  if (colon == std::string_view::npos || colon == 0 || !IsAlpha(text[0])) {
    return std::nullopt;
  }
  std::string scheme;
  for (const char c : text.substr(0, colon)) {
    if (!IsAlpha(c) && !IsDigit(c) && c != '+' && c != '-' && c != '.') {
      return std::nullopt;
    }
    scheme.push_back(ToLower(c));
  }
  return scheme;
}

bool IsRejectedScheme(std::string_view scheme, std::string_view rest) {
  if (rest.starts_with("//") || scheme.starts_with("chrome")) {
    return true;
  }
  return std::find(std::begin(kRejectedSchemes), std::end(kRejectedSchemes),
                   scheme) != std::end(kRejectedSchemes);
}

UrlInput Url(std::string_view url) {
  return UrlInput{UrlInputKind::kUrl, std::string(url), {}};
}

}  // namespace

std::string SearchUrl(std::string_view query) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string url(kDefaultSearchUrlPrefix);
  for (const char c : query) {
    if (IsAlpha(c) || IsDigit(c) || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      url.push_back(c);
    } else if (c == ' ') {
      url.push_back('+');
    } else {
      const auto byte = static_cast<unsigned char>(c);
      url.push_back('%');
      url.push_back(kHex[byte >> 4]);
      url.push_back(kHex[byte & 0x0F]);
    }
  }
  return url;
}

UrlInput ClassifyUrlInput(std::string_view text) {
  text = Trim(text);
  if (text.empty()) {
    return UrlInput{};
  }
  if (StartsWithIgnoreCase(text, "http://") ||
      StartsWithIgnoreCase(text, "https://") ||
      EqualsIgnoreCase(text, "about:blank")) {
    return Url(text);
  }
  const bool has_space = std::any_of(text.begin(), text.end(), IsSpace);
  if (!has_space) {
    if (LooksLikeHost(text)) {
      return Url("https://" + std::string(text));
    }
    if (const std::optional<std::string> scheme = SchemeOf(text)) {
      const std::string_view rest = text.substr(scheme->size() + 1);
      if (IsRejectedScheme(*scheme, rest)) {
        return UrlInput{
            UrlInputKind::kRejected, {}, "Cannot open " + *scheme + ": URLs"};
      }
    }
  }
  return UrlInput{UrlInputKind::kSearch, SearchUrl(text), {}};
}

}  // namespace vrowser
