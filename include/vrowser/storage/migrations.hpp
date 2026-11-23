// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

struct sqlite3;

namespace vrowser {

// One schema step. Versions start at 1 and increase by one; the database
// records the last version it applied.
struct Migration {
  int version = 0;
  std::string_view sql;
};

// Outcome of opening or migrating a database. Errors are values, not
// exceptions, so the module stays usable from targets built with
// -fno-exceptions.
struct MigrationResult {
  bool ok = false;
  // The schema version the database is at after the call.
  int version = 0;
  // Empty when ok.
  std::string error;
};

// An open SQLite database file. Move-only; closes the file when destroyed.
class Database {
 public:
  // Opens or creates the file at `path`. The parent directory must exist.
  // On failure the returned database is not open and `error` says why.
  static Database Open(const std::filesystem::path& path, std::string* error);

  Database();
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) noexcept;
  Database& operator=(Database&&) noexcept;
  ~Database();

  [[nodiscard]] bool is_open() const { return handle_ != nullptr; }
  [[nodiscard]] sqlite3* handle() const { return handle_.get(); }

  // The version recorded in the file, 0 for a new one. -1 when not open or
  // the version cannot be read.
  [[nodiscard]] int SchemaVersion() const;

 private:
  struct Closer {
    void operator()(sqlite3* db) const;
  };

  std::unique_ptr<sqlite3, Closer> handle_;
};

// Applies the migrations newer than the database's version, in order, each in
// its own transaction with the version update. A migration that fails is
// rolled back and leaves the version of the last one that succeeded. The list
// must be numbered 1..N. A database newer than the list is left untouched and
// reported as an error.
MigrationResult ApplyMigrations(Database& db,
                                std::span<const Migration> migrations);

}  // namespace vrowser
