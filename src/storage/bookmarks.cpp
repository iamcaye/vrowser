// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/bookmarks.hpp"

#include <sqlite3.h>

#include "statement.hpp"
#include "vrowser/storage/history.hpp"
#include "vrowser/storage/profile_database.hpp"

namespace vrowser {

using storage_internal::Statement;

BookmarkStore BookmarkStore::Open(const std::filesystem::path& path,
                                  std::string* error) {
  BookmarkStore store;
  store.db_ = OpenProfileDatabase(path, error);
  return store;
}

BookmarkAddResult BookmarkStore::Add(std::string_view url,
                                     std::string_view title,
                                     std::int64_t created_at_ms) {
  if (!IsHistoryUrl(url)) {
    return BookmarkAddResult::kNotWebUrl;
  }
  if (!is_open()) {
    return BookmarkAddResult::kFailed;
  }
  Statement insert(db_.handle(),
                   "INSERT INTO bookmarks(url, title, created_at_ms) "
                   "VALUES (?1, ?2, ?3) ON CONFLICT(url) DO NOTHING");
  if (!insert.ok() || !insert.BindText(1, url) || !insert.BindText(2, title) ||
      !insert.BindInt64(3, created_at_ms) ||
      sqlite3_step(insert.get()) != SQLITE_DONE) {
    return BookmarkAddResult::kFailed;
  }
  return sqlite3_changes(db_.handle()) == 1
             ? BookmarkAddResult::kAdded
             : BookmarkAddResult::kAlreadyPresent;
}

bool BookmarkStore::Remove(std::string_view url) {
  if (!is_open()) {
    return false;
  }
  Statement remove(db_.handle(), "DELETE FROM bookmarks WHERE url = ?1");
  if (!remove.ok() || !remove.BindText(1, url) ||
      sqlite3_step(remove.get()) != SQLITE_DONE) {
    return false;
  }
  return sqlite3_changes(db_.handle()) == 1;
}

bool BookmarkStore::Contains(std::string_view url) const {
  if (!is_open()) {
    return false;
  }
  Statement select(db_.handle(), "SELECT 1 FROM bookmarks WHERE url = ?1");
  return select.ok() && select.BindText(1, url) &&
         sqlite3_step(select.get()) == SQLITE_ROW;
}

std::vector<Bookmark> BookmarkStore::List() const {
  std::vector<Bookmark> bookmarks;
  if (!is_open()) {
    return bookmarks;
  }
  Statement select(db_.handle(),
                   "SELECT id, url, title, created_at_ms FROM bookmarks "
                   "ORDER BY created_at_ms, id");
  if (!select.ok()) {
    return bookmarks;
  }
  while (sqlite3_step(select.get()) == SQLITE_ROW) {
    bookmarks.push_back({
        .id = sqlite3_column_int64(select.get(), 0),
        .url = select.ColumnText(1),
        .title = select.ColumnText(2),
        .created_at_ms = sqlite3_column_int64(select.get(), 3),
    });
  }
  return bookmarks;
}

}  // namespace vrowser
