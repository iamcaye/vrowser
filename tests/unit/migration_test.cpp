// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/migrations.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace vrowser {
namespace {

namespace fs = std::filesystem;

constexpr Migration kCreateItems{
    .version = 1,
    .sql = "CREATE TABLE items(id INTEGER)",
};
constexpr Migration kSeedItems{
    .version = 2,
    .sql = "INSERT INTO items VALUES (1)",
};
constexpr std::array<Migration, 2> kSchema = {kCreateItems, kSeedItems};

class MigrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> counter{0};
    dir_ = fs::temp_directory_path() /
           ("vrowser-migration-" +
            std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "-" + std::to_string(counter++));
    fs::remove_all(dir_);
    fs::create_directories(dir_);
    path_ = dir_ / "profile.db";
  }

  void TearDown() override { fs::remove_all(dir_); }

  Database OpenOrFail() {
    std::string error;
    Database db = Database::Open(path_, &error);
    EXPECT_TRUE(db.is_open()) << error;
    return db;
  }

  // Runs a single-value query, or -1 when it fails.
  static int QueryInt(Database& db, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db.handle(), sql, -1, &statement, nullptr) !=
        SQLITE_OK) {
      return -1;
    }
    int value = -1;
    if (sqlite3_step(statement) == SQLITE_ROW) {
      value = sqlite3_column_int(statement, 0);
    }
    sqlite3_finalize(statement);
    return value;
  }

  static int TableCount(Database& db, const char* name) {
    const std::string sql =
        std::string("SELECT count(*) FROM sqlite_schema WHERE name = '") +
        name + "'";
    return QueryInt(db, sql.c_str());
  }

  [[nodiscard]] const fs::path& dir() const { return dir_; }

 private:
  fs::path dir_;
  fs::path path_;
};

TEST_F(MigrationTest, FreshFileEndsAtLatestVersion) {
  Database db = OpenOrFail();
  EXPECT_EQ(db.SchemaVersion(), 0);

  const MigrationResult result = ApplyMigrations(db, kSchema);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.version, 2);
  EXPECT_EQ(db.SchemaVersion(), 2);
  EXPECT_EQ(QueryInt(db, "SELECT count(*) FROM items"), 1);
}

TEST_F(MigrationTest, SecondOpenDoesNotReapplyMigrations) {
  {
    Database db = OpenOrFail();
    ASSERT_TRUE(ApplyMigrations(db, kSchema).ok);
  }

  Database db = OpenOrFail();
  EXPECT_EQ(db.SchemaVersion(), 2);
  // Reapplying would fail on CREATE TABLE or insert a second row.
  const MigrationResult result = ApplyMigrations(db, kSchema);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.version, 2);
  EXPECT_EQ(QueryInt(db, "SELECT count(*) FROM items"), 1);
}

TEST_F(MigrationTest, NewMigrationAppliesOnlyTheNewStep) {
  {
    Database db = OpenOrFail();
    ASSERT_TRUE(ApplyMigrations(db, std::vector<Migration>{kCreateItems}).ok);
  }

  Database db = OpenOrFail();
  const MigrationResult result = ApplyMigrations(db, kSchema);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.version, 2);
  EXPECT_EQ(QueryInt(db, "SELECT count(*) FROM items"), 1);
}

TEST_F(MigrationTest, FailingMigrationDoesNotAdvanceTheVersion) {
  Database db = OpenOrFail();
  const std::vector<Migration> migrations = {
      kCreateItems,
      // The first statement succeeds; the second fails, so both roll back.
      {
          .version = 2,
          .sql = "CREATE TABLE partial(x); INSERT INTO missing VALUES (1)",
      },
      {.version = 3, .sql = "CREATE TABLE never(x)"},
  };

  const MigrationResult result = ApplyMigrations(db, migrations);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.version, 1);
  EXPECT_NE(result.error.find("Migration 2 failed"), std::string::npos)
      << result.error;
  EXPECT_EQ(db.SchemaVersion(), 1);
  EXPECT_EQ(TableCount(db, "items"), 1);
  EXPECT_EQ(TableCount(db, "partial"), 0);
  EXPECT_EQ(TableCount(db, "never"), 0);
}

TEST_F(MigrationTest, FixedMigrationAppliesAfterAFailure) {
  Database db = OpenOrFail();
  const std::vector<Migration> broken = {
      kCreateItems,
      {.version = 2, .sql = "INSERT INTO missing VALUES (1)"},
  };
  ASSERT_FALSE(ApplyMigrations(db, broken).ok);

  const MigrationResult result = ApplyMigrations(db, kSchema);

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.version, 2);
}

TEST_F(MigrationTest, RejectsMigrationsThatAreNotNumberedFromOne) {
  Database db = OpenOrFail();
  const std::vector<Migration> gap = {
      kCreateItems,
      {.version = 3, .sql = "SELECT 1"},
  };

  const MigrationResult result = ApplyMigrations(db, gap);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.version, 0);
  EXPECT_EQ(TableCount(db, "items"), 0);
}

TEST_F(MigrationTest, LeavesANewerDatabaseUntouched) {
  Database db = OpenOrFail();
  ASSERT_TRUE(ApplyMigrations(db, kSchema).ok);

  const MigrationResult result =
      ApplyMigrations(db, std::vector<Migration>{kCreateItems});

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.version, 2);
  EXPECT_EQ(db.SchemaVersion(), 2);
}

TEST_F(MigrationTest, OpenReportsAMissingDirectory) {
  std::string error;
  Database db = Database::Open(dir() / "missing" / "profile.db", &error);

  EXPECT_FALSE(db.is_open());
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(db.SchemaVersion(), -1);
  EXPECT_FALSE(ApplyMigrations(db, kSchema).ok);
}

TEST_F(MigrationTest, OpenRefusesASymlinkedDatabaseFile) {
  {
    Database target = OpenOrFail();
  }
  std::error_code ec;
  fs::create_symlink(dir() / "profile.db", dir() / "link.db", ec);
  if (ec) {
    GTEST_SKIP() << "Cannot create a symlink here: " << ec.message();
  }

  std::string error;
  Database db = Database::Open(dir() / "link.db", &error);

  EXPECT_FALSE(db.is_open());
  EXPECT_FALSE(error.empty());
}

}  // namespace
}  // namespace vrowser
