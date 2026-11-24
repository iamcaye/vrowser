// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace vrowser::storage_internal {

// A prepared statement that finalizes itself. Private to src/storage.
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

  // A text column of the current row, empty for NULL.
  [[nodiscard]] std::string ColumnText(int column) const {
    const unsigned char* text = sqlite3_column_text(statement_, column);
    if (text == nullptr) {
      return {};
    }
    return {reinterpret_cast<const char*>(text),
            static_cast<size_t>(sqlite3_column_bytes(statement_, column))};
  }

 private:
  sqlite3_stmt* statement_ = nullptr;
};

}  // namespace vrowser::storage_internal
