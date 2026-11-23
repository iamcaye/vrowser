// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/profile.hpp"

#include <string>

namespace vrowser {

bool IsValidProfileName(std::string_view name) {
  constexpr size_t kMaxLength = 64;
  if (name.empty() || name.size() > kMaxLength || name.front() == '-') {
    return false;
  }
  for (char c : name) {
    const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                         c == '_' || c == '-';
    if (!allowed) {
      return false;
    }
  }
  return true;
}

std::optional<std::filesystem::path> ProfileDirectory(
    const std::filesystem::path& root,
    std::string_view name) {
  if (!IsValidProfileName(name)) {
    return std::nullopt;
  }
  if (name == kDefaultProfileName) {
    return root / std::string(kDefaultProfileDirectoryName);
  }
  return root / (std::string(kProfileDirectoryPrefix) + std::string(name));
}

}  // namespace vrowser
