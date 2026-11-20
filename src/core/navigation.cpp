// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/navigation.hpp"

#include <cctype>

namespace vrowser {
namespace {

bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
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

bool HasJavaScriptScheme(std::string_view url) {
  constexpr std::string_view kScheme = "javascript:";
  if (url.size() < kScheme.size()) {
    return false;
  }
  for (size_t i = 0; i < kScheme.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(url[i])) != kScheme[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool NavigationController::Request(std::string_view url) {
  url = Trim(url);
  if (url.empty() || HasJavaScriptScheme(url)) {
    return false;
  }
  requested_url_.assign(url);
  ++sequence_;
  if (listener_) {
    listener_(requested_url_);
  }
  return true;
}

}  // namespace vrowser
