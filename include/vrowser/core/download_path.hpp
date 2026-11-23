// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace vrowser {

// The directory under the profile that every download is saved into.
inline constexpr std::string_view kDownloadsDirectoryName = "Downloads";

// A file name from a page (Content-Disposition, the URL, or a `download`
// attribute), made safe to use as one entry in the downloads directory. The
// name is untrusted:
//   - only the part after the last `/` or `\` is kept, so `../../x` is `x`;
//   - control characters and `<>:"|?*` become `_`;
//   - a leading `.` becomes `_`, so `.` and `..` cannot name a directory
//     and nothing is hidden; trailing dots and spaces are dropped;
//   - Windows device names (`CON`, `NUL`, `COM1`, ...) get a leading `_`;
//   - the name is cut to 200 bytes on a UTF-8 boundary, keeping a short
//     extension.
// An empty result becomes `download`. The result is never empty and never
// contains a path separator.
std::string SanitizeDownloadName(std::string_view suggested);

// Returns `dir / name`, or `dir / "stem (n).ext"` with the smallest n from 1
// that `taken` reports free. `name` must come from SanitizeDownloadName.
// Returns nullopt after 999 taken names.
std::optional<std::filesystem::path> UniqueDownloadPath(
    const std::filesystem::path& dir,
    std::string_view name,
    const std::function<bool(const std::filesystem::path&)>& taken);

// Whether `path` is an entry somewhere below `dir`, compared lexically after
// normalization. Both should be absolute. `dir` itself is not inside.
bool IsInsideDirectory(const std::filesystem::path& dir,
                       const std::filesystem::path& path);

}  // namespace vrowser
