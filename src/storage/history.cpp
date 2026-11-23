// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/history.hpp"

#include <sqlite3.h>

#include <array>
#include <cctype>
#include <utility>

namespace vrowser {
namespace {

constexpr std::array<Migration, 1> kProfileMigrations = {
    Migration{
        .version = 1,
        .sql = "CREATE TABLE history("
               "  id INTEGER PRIMARY KEY,"
               "  url TEXT NOT NULL,"
               "  title TEXT NOT NULL DEFAULT '',"
               "  visited_at_ms INTEGER NOT NULL);"
               "CREATE INDEX history_visited_at ON history(visited_at_ms)",
    },
};

// A prepared statement that finalizes itself.
class Statement {
 public:
  Statement(sqlite3* db, const char* sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      statement_ = nullptr;
    }
  }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&&) = delete;
  Statement& operator=(Statement&&) = delete;
  ~Statement() { sqlite3_finalize(statement_); }

  [[nodiscard]] bool ok() const { return statement_ != nullptr; }
  [[nodiscard]] sqlite3_stmt* get() const { return statement_; }

  bool BindText(int index, std::string_view text) {
    return sqlite3_bind_text(statement_, index, text.data(),
                             static_cast<int>(text.size()),
                             SQLITE_TRANSIENT) == SQLITE_OK;
  }
  bool BindInt64(int index, std::int64_t value) {
    return sqlite3_bind_int64(statement_, index, value) == SQLITE_OK;
  }

 private:
  sqlite3_stmt* statement_ = nullptr;
};

std::string ColumnText(sqlite3_stmt* statement, int column) {
  const unsigned char* text = sqlite3_column_text(statement, column);
  if (text == nullptr) {
    return {};
  }
  return {reinterpret_cast<const char*>(text),
          static_cast<size_t>(sqlite3_column_bytes(statement, column))};
}

bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(text[i])) != prefix[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

HistoryStore HistoryStore::Open(const std::filesystem::path& path,
                                std::string* error) {
  HistoryStore store;
  std::string open_error;
  store.db_ = Database::Open(path, &open_error);
  if (!store.db_.is_open()) {
    if (error != nullptr) {
      *error = std::move(open_error);
    }
    return store;
  }
  MigrationResult migrated = ApplyMigrations(store.db_, kProfileMigrations);
  if (!migrated.ok) {
    if (error != nullptr) {
      *error = std::move(migrated.error);
    }
    store.db_ = Database();
  }
  return store;
}

std::int64_t HistoryStore::AddVisit(std::string_view url,
                                    std::string_view title,
                                    std::int64_t visited_at_ms) {
  if (!is_open()) {
    return 0;
  }
  Statement insert(db_.handle(),
                   "INSERT INTO history(url, title, visited_at_ms) "
                   "VALUES (?1, ?2, ?3)");
  if (!insert.ok() || !insert.BindText(1, url) || !insert.BindText(2, title) ||
      !insert.BindInt64(3, visited_at_ms) ||
      sqlite3_step(insert.get()) != SQLITE_DONE) {
    return 0;
  }
  return sqlite3_last_insert_rowid(db_.handle());
}

bool HistoryStore::SetTitle(std::int64_t id, std::string_view title) {
  if (!is_open()) {
    return false;
  }
  Statement update(db_.handle(), "UPDATE history SET title = ?1 WHERE id = ?2");
  if (!update.ok() || !update.BindText(1, title) || !update.BindInt64(2, id) ||
      sqlite3_step(update.get()) != SQLITE_DONE) {
    return false;
  }
  return sqlite3_changes(db_.handle()) == 1;
}

std::vector<HistoryEntry> HistoryStore::Newest(int limit) const {
  std::vector<HistoryEntry> entries;
  if (!is_open() || limit <= 0) {
    return entries;
  }
  Statement select(db_.handle(),
                   "SELECT id, url, title, visited_at_ms FROM history "
                   "ORDER BY visited_at_ms DESC, id DESC LIMIT ?1");
  if (!select.ok() || !select.BindInt64(1, limit)) {
    return entries;
  }
  while (sqlite3_step(select.get()) == SQLITE_ROW) {
    entries.push_back({
        .id = sqlite3_column_int64(select.get(), 0),
        .url = ColumnText(select.get(), 1),
        .title = ColumnText(select.get(), 2),
        .visited_at_ms = sqlite3_column_int64(select.get(), 3),
    });
  }
  return entries;
}

bool IsHistoryUrl(std::string_view url) {
  return StartsWithIgnoringCase(url, "http://") ||
         StartsWithIgnoringCase(url, "https://");
}

bool HistoryRecorder::OnCommit(int tab,
                               bool main_frame,
                               std::string_view url,
                               std::int64_t now_ms) {
  if (!main_frame) {
    return false;
  }
  // A non-history page still ends the previous visit's title updates.
  last_visit_.erase(tab);
  if (!IsHistoryUrl(url)) {
    return false;
  }
  const std::int64_t id = store_.AddVisit(url, "", now_ms);
  if (id == 0) {
    return false;
  }
  last_visit_[tab] = id;
  return true;
}

void HistoryRecorder::OnTitle(int tab, std::string_view title) {
  const auto it = last_visit_.find(tab);
  if (it != last_visit_.end()) {
    store_.SetTitle(it->second, title);
  }
}

}  // namespace vrowser
