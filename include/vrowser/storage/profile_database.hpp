// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "vrowser/storage/migrations.hpp"

namespace vrowser {

// The profile database file, in the profile directory.
inline constexpr std::string_view kProfileDatabaseFileName = "vrowser.db";

// The schema of the profile database: history, then bookmarks. Every store
// that uses the file opens it with this one list.
std::span<const Migration> ProfileMigrations();

// Opens or creates the profile database at `path` and brings its schema up to
// date. On failure the database is not open and `error` says why.
Database OpenProfileDatabase(const std::filesystem::path& path,
                             std::string* error);

}  // namespace vrowser
