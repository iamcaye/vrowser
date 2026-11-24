// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/history.hpp"

#include <sqlite3.h>

#include <cctype>
#include <utility>

#include "statement.hpp"
#include "vrowser/storage/profile_database.hpp"

namespace vrowser {
namespace {

using storage_internal::Statement;

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
  store.db_ = OpenProfileDatabase(path, error);
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
        .url = select.ColumnText(1),
        .title = select.ColumnText(2),
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
