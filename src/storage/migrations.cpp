// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/migrations.hpp"

#include <sqlite3.h>

#include <system_error>
#include <utility>

namespace vrowser {
namespace {

// Runs SQL that returns no rows. On failure, returns false and fills `error`.
bool Exec(sqlite3* db, const std::string& sql, std::string* error) {
  char* message = nullptr;
  const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &message);
  if (rc == SQLITE_OK) {
    return true;
  }
  *error = message != nullptr ? message : sqlite3_errstr(rc);
  sqlite3_free(message);
  return false;
}

MigrationResult Failure(int version, std::string error) {
  return {.ok = false, .version = version, .error = std::move(error)};
}

// Returns an empty string when the versions are 1..N, else the problem.
std::string CheckNumbering(std::span<const Migration> migrations) {
  int expected = 1;
  for (const Migration& migration : migrations) {
    if (migration.version != expected) {
      return "Migration " + std::to_string(expected) +
             " is missing or out of order (found " +
             std::to_string(migration.version) + ")";
    }
    ++expected;
  }
  return {};
}

// Applies one migration and records its version in one transaction.
bool ApplyOne(sqlite3* db, const Migration& migration, std::string* error) {
  if (!Exec(db, "BEGIN IMMEDIATE", error)) {
    return false;
  }
  const std::string body =
      std::string(migration.sql) +
      ";\nPRAGMA user_version = " + std::to_string(migration.version) + ";";
  if (!Exec(db, body, error) || !Exec(db, "COMMIT", error)) {
    std::string ignored;
    Exec(db, "ROLLBACK", &ignored);
    *error =
        "Migration " + std::to_string(migration.version) + " failed: " + *error;
    return false;
  }
  return true;
}

}  // namespace

void Database::Closer::operator()(sqlite3* db) const {
  sqlite3_close_v2(db);
}

Database::Database() = default;
Database::Database(Database&&) noexcept = default;
Database& Database::operator=(Database&&) noexcept = default;
Database::~Database() = default;

Database Database::Open(const std::filesystem::path& path, std::string* error) {
  Database result;
  // SQLite's NOFOLLOW refuses a symlink anywhere in the path, and system
  // directories such as macOS's /var are symlinks, so resolve the parent
  // first. The database file itself must still not be a symlink.
  std::error_code ec;
  const std::filesystem::path parent = std::filesystem::canonical(
      path.has_parent_path() ? path.parent_path() : std::filesystem::path("."),
      ec);
  if (ec) {
    if (error != nullptr) {
      *error =
          "Cannot open the directory of " + path.string() + ": " + ec.message();
    }
    return result;
  }
  const std::string resolved = (parent / path.filename()).string();
  sqlite3* raw = nullptr;
  const int flags =
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOFOLLOW;
  const int rc = sqlite3_open_v2(resolved.c_str(), &raw, flags, nullptr);
  // sqlite3_open_v2 can return a handle even on failure; it must be closed.
  result.handle_.reset(raw);
  if (rc != SQLITE_OK) {
    if (error != nullptr) {
      *error = raw != nullptr ? sqlite3_errmsg(raw) : sqlite3_errstr(rc);
    }
    result.handle_.reset();
  }
  return result;
}

int Database::SchemaVersion() const {
  if (!handle_) {
    return -1;
  }
  sqlite3_stmt* statement = nullptr;
  if (sqlite3_prepare_v2(handle_.get(), "PRAGMA user_version", -1, &statement,
                         nullptr) != SQLITE_OK) {
    return -1;
  }
  int version = -1;
  if (sqlite3_step(statement) == SQLITE_ROW) {
    version = sqlite3_column_int(statement, 0);
  }
  sqlite3_finalize(statement);
  return version;
}

MigrationResult ApplyMigrations(Database& db,
                                std::span<const Migration> migrations) {
  if (!db.is_open()) {
    return Failure(-1, "Database is not open");
  }
  int version = db.SchemaVersion();
  if (version < 0) {
    return Failure(version, sqlite3_errmsg(db.handle()));
  }
  if (std::string problem = CheckNumbering(migrations); !problem.empty()) {
    return Failure(version, std::move(problem));
  }
  const int latest = static_cast<int>(migrations.size());
  if (version > latest) {
    return Failure(version, "Database version " + std::to_string(version) +
                                " is newer than this build (" +
                                std::to_string(latest) + ")");
  }
  for (const Migration& migration : migrations.subspan(version)) {
    std::string error;
    if (!ApplyOne(db.handle(), migration, &error)) {
      return Failure(db.SchemaVersion(), std::move(error));
    }
    version = migration.version;
  }
  return {.ok = true, .version = version, .error = {}};
}

}  // namespace vrowser
