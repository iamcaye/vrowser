// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/download_path.hpp"

#include <array>
#include <cctype>
#include <cstddef>

namespace vrowser {
namespace {

constexpr size_t kMaxNameBytes = 200;
constexpr size_t kMaxExtensionBytes = 20;
constexpr int kMaxSuffix = 999;
constexpr std::string_view kFallbackName = "download";

bool IsForbidden(unsigned char c) {
  constexpr std::string_view kReserved = "<>:\"|?*/\\";
  return c < 0x20 || c == 0x7f ||
         kReserved.find(static_cast<char>(c)) != std::string_view::npos;
}

std::string ToUpperAscii(std::string_view text) {
  std::string upper(text);
  for (char& c : upper) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return upper;
}

// CON, PRN, AUX, NUL, COM1-9, LPT1-9, with or without an extension.
bool IsWindowsDeviceName(std::string_view name) {
  const std::string stem = ToUpperAscii(name.substr(0, name.find('.')));
  constexpr std::array<std::string_view, 4> kDevices = {"CON", "PRN", "AUX",
                                                        "NUL"};
  for (std::string_view device : kDevices) {
    if (stem == device) {
      return true;
    }
  }
  return stem.size() == 4 &&
         (stem.starts_with("COM") || stem.starts_with("LPT")) &&
         stem[3] >= '1' && stem[3] <= '9';
}

// The longest prefix of `text` of at most `max` bytes that does not end inside
// a UTF-8 sequence.
std::string_view Utf8Prefix(std::string_view text, size_t max) {
  if (text.size() <= max) {
    return text;
  }
  size_t end = max;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
    --end;
  }
  return text.substr(0, end);
}

std::string Truncate(const std::string& name) {
  if (name.size() <= kMaxNameBytes) {
    return name;
  }
  const size_t dot = name.rfind('.');
  if (dot != std::string::npos && dot > 0 &&
      name.size() - dot <= kMaxExtensionBytes) {
    const std::string_view extension = std::string_view(name).substr(dot);
    return std::string(Utf8Prefix(name, kMaxNameBytes - extension.size())) +
           std::string(extension);
  }
  return std::string(Utf8Prefix(name, kMaxNameBytes));
}

std::filesystem::path FromUtf8(std::string_view text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

}  // namespace

std::string SanitizeDownloadName(std::string_view suggested) {
  const size_t slash = suggested.find_last_of("/\\");
  if (slash != std::string_view::npos) {
    suggested.remove_prefix(slash + 1);
  }
  std::string name;
  name.reserve(suggested.size());
  for (char c : suggested) {
    name += IsForbidden(static_cast<unsigned char>(c)) ? '_' : c;
  }
  // Trailing dots and spaces are dropped by Windows; leading spaces are
  // confusing everywhere.
  while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
    name.pop_back();
  }
  const size_t first = name.find_first_not_of(' ');
  name.erase(0, first == std::string::npos ? name.size() : first);
  if (name.empty()) {
    return std::string(kFallbackName);
  }
  if (name.front() == '.') {
    name.front() = '_';
  }
  if (IsWindowsDeviceName(name)) {
    name.insert(0, "_");
  }
  return Truncate(name);
}

std::optional<std::filesystem::path> UniqueDownloadPath(
    const std::filesystem::path& dir,
    std::string_view name,
    const std::function<bool(const std::filesystem::path&)>& taken) {
  if (name.empty() || name.find_first_of("/\\") != std::string_view::npos ||
      name == "." || name == "..") {
    return std::nullopt;
  }
  std::filesystem::path path = dir / FromUtf8(name);
  if (!taken(path)) {
    return path;
  }
  // `archive.tar.gz` becomes `archive (1).tar.gz` only for the last dot, which
  // is fine: the name stays unique and keeps its type.
  const size_t dot = name.rfind('.');
  const bool has_extension = dot != std::string_view::npos && dot > 0;
  const std::string_view stem = has_extension ? name.substr(0, dot) : name;
  const std::string_view extension =
      has_extension ? name.substr(dot) : std::string_view();
  for (int n = 1; n <= kMaxSuffix; ++n) {
    const std::string candidate = std::string(stem) + " (" + std::to_string(n) +
                                  ")" + std::string(extension);
    path = dir / FromUtf8(candidate);
    if (!taken(path)) {
      return path;
    }
  }
  return std::nullopt;
}

bool IsInsideDirectory(const std::filesystem::path& dir,
                       const std::filesystem::path& path) {
  const std::filesystem::path relative =
      path.lexically_normal().lexically_relative(dir.lexically_normal());
  if (relative.empty() || relative == ".") {
    return false;
  }
  return *relative.begin() != "..";
}

}  // namespace vrowser
