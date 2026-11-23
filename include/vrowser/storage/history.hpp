// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "vrowser/storage/migrations.hpp"

namespace vrowser {

// The profile database file, in the profile directory.
inline constexpr std::string_view kProfileDatabaseFileName = "vrowser.db";

// One visit: a committed main-frame navigation.
struct HistoryEntry {
  std::int64_t id = 0;
  std::string url;
  std::string title;
  // Milliseconds since the Unix epoch.
  std::int64_t visited_at_ms = 0;

  bool operator==(const HistoryEntry&) const = default;
};

// Visits in the profile database. The SQL stays here.
class HistoryStore {
 public:
  // Opens or creates the database at `path` and brings its schema up to date.
  // On failure the store is not open and `error` says why.
  static HistoryStore Open(const std::filesystem::path& path,
                           std::string* error);

  [[nodiscard]] bool is_open() const { return db_.is_open(); }

  // Adds a visit. Returns its row ID, or 0 on failure.
  std::int64_t AddVisit(std::string_view url,
                        std::string_view title,
                        std::int64_t visited_at_ms);

  // Replaces the title of a visit. Returns false when the row is missing or
  // the write fails.
  bool SetTitle(std::int64_t id, std::string_view title);

  // Up to `limit` visits, newest first.
  [[nodiscard]] std::vector<HistoryEntry> Newest(int limit) const;

 private:
  Database db_;
};

// Whether a committed URL belongs in history: http and https only, so error
// pages (data:), about:, and browser-internal pages are left out.
bool IsHistoryUrl(std::string_view url);

// Turns load events from the browser into visits. Knows tabs only by ID.
class HistoryRecorder {
 public:
  explicit HistoryRecorder(HistoryStore store) : store_(std::move(store)) {}

  // A navigation committed in `tab`. Subframes and non-history URLs are
  // ignored. Returns true when a visit was written.
  bool OnCommit(int tab,
                bool main_frame,
                std::string_view url,
                std::int64_t now_ms);

  // The page title in `tab` changed; it names that tab's latest visit.
  void OnTitle(int tab, std::string_view title);

  void OnTabClosed(int tab) { last_visit_.erase(tab); }

  [[nodiscard]] const HistoryStore& store() const { return store_; }

 private:
  HistoryStore store_;
  // The latest visit per tab, for titles that arrive after the commit.
  std::map<int, std::int64_t> last_visit_;
};

}  // namespace vrowser
