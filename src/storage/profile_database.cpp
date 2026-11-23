// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/profile_database.hpp"

#include <array>
#include <utility>

namespace vrowser {
namespace {

// Append only: a released migration never changes.
constexpr std::array<Migration, 2> kProfileMigrations = {
    Migration{
        .version = 1,
        .sql = "CREATE TABLE history("
               "  id INTEGER PRIMARY KEY,"
               "  url TEXT NOT NULL,"
               "  title TEXT NOT NULL DEFAULT '',"
               "  visited_at_ms INTEGER NOT NULL);"
               "CREATE INDEX history_visited_at ON history(visited_at_ms)",
    },
    Migration{
        .version = 2,
        .sql = "CREATE TABLE bookmarks("
               "  id INTEGER PRIMARY KEY,"
               "  url TEXT NOT NULL UNIQUE,"
               "  title TEXT NOT NULL DEFAULT '',"
               "  created_at_ms INTEGER NOT NULL)",
    },
};

}  // namespace

std::span<const Migration> ProfileMigrations() {
  return kProfileMigrations;
}

Database OpenProfileDatabase(const std::filesystem::path& path,
                             std::string* error) {
  std::string open_error;
  Database db = Database::Open(path, &open_error);
  if (!db.is_open()) {
    if (error != nullptr) {
      *error = std::move(open_error);
    }
    return db;
  }
  MigrationResult migrated = ApplyMigrations(db, kProfileMigrations);
  if (!migrated.ok) {
    if (error != nullptr) {
      *error = std::move(migrated.error);
    }
    return Database();
  }
  return db;
}

}  // namespace vrowser
