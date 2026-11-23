// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "vrowser/storage/migrations.hpp"

namespace vrowser {

// A saved URL. One row per URL; no folders.
struct Bookmark {
  std::int64_t id = 0;
  std::string url;
  std::string title;
  // Milliseconds since the Unix epoch.
  std::int64_t created_at_ms = 0;

  bool operator==(const Bookmark&) const = default;
};

enum class BookmarkAddResult {
  kAdded,
  // The URL is already bookmarked; the row is unchanged.
  kAlreadyPresent,
  // Only http and https URLs are bookmarked.
  kNotWebUrl,
  kFailed,
};

// Bookmarks in the profile database. The SQL stays here.
class BookmarkStore {
 public:
  // Opens or creates the profile database at `path` and brings its schema up
  // to date. On failure the store is not open and `error` says why.
  static BookmarkStore Open(const std::filesystem::path& path,
                            std::string* error);

  [[nodiscard]] bool is_open() const { return db_.is_open(); }

  BookmarkAddResult Add(std::string_view url,
                        std::string_view title,
                        std::int64_t created_at_ms);

  // Returns true when a bookmark for exactly `url` was removed.
  bool Remove(std::string_view url);

  [[nodiscard]] bool Contains(std::string_view url) const;

  // Every bookmark, oldest first.
  [[nodiscard]] std::vector<Bookmark> List() const;

 private:
  Database db_;
};

}  // namespace vrowser
