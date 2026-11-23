// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace vrowser {

// The profile used when --profile is not given.
inline constexpr std::string_view kDefaultProfileName = "default";

// Chromium creates profiles only as direct children of the user-data
// directory. The default profile keeps Chromium's own `Default` directory;
// another profile `<name>` is `Profile-<name>`.
inline constexpr std::string_view kDefaultProfileDirectoryName = "Default";
inline constexpr std::string_view kProfileDirectoryPrefix = "Profile-";

// 1 to 64 characters of `a`-`z`, `0`-`9`, `_`, and `-`, not starting with
// `-`. Lowercase only, so two names cannot share one directory on a
// case-insensitive file system.
bool IsValidProfileName(std::string_view name);

// `root / Default` for the default profile, `root / Profile-<name>`
// otherwise, or nullopt when `name` is not valid. Each profile's cookies,
// cache, and Vrowser database live there.
std::optional<std::filesystem::path> ProfileDirectory(
    const std::filesystem::path& root,
    std::string_view name);

}  // namespace vrowser
