// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/bookmarks.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

#include "vrowser/storage/history.hpp"
#include "vrowser/storage/profile_database.hpp"

namespace vrowser {
namespace {

namespace fs = std::filesystem;

class BookmarksTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> counter{0};
    dir_ = fs::temp_directory_path() /
           ("vrowser-bookmarks-" +
            std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "-" + std::to_string(counter++));
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  [[nodiscard]] fs::path DatabasePath() const {
    return dir_ / std::string(kProfileDatabaseFileName);
  }

  [[nodiscard]] BookmarkStore OpenStore() const {
    std::string error;
    BookmarkStore store = BookmarkStore::Open(DatabasePath(), &error);
    EXPECT_TRUE(store.is_open()) << error;
    return store;
  }

 private:
  fs::path dir_;
};

TEST_F(BookmarksTest, AddThenListReturnsBookmarksOldestFirst) {
  BookmarkStore store = OpenStore();
  EXPECT_EQ(store.Add("https://b.example/", "B", 2000),
            BookmarkAddResult::kAdded);
  EXPECT_EQ(store.Add("https://a.example/", "A", 1000),
            BookmarkAddResult::kAdded);

  const std::vector<Bookmark> list = store.List();

  ASSERT_EQ(list.size(), 2U);
  EXPECT_EQ(list[0].url, "https://a.example/");
  EXPECT_EQ(list[0].title, "A");
  EXPECT_EQ(list[0].created_at_ms, 1000);
  EXPECT_EQ(list[1].url, "https://b.example/");
  EXPECT_TRUE(store.Contains("https://a.example/"));
}

TEST_F(BookmarksTest, AddingTheSameUrlTwiceKeepsOneRow) {
  BookmarkStore store = OpenStore();
  ASSERT_EQ(store.Add("https://a.example/", "First", 1000),
            BookmarkAddResult::kAdded);

  EXPECT_EQ(store.Add("https://a.example/", "Second", 2000),
            BookmarkAddResult::kAlreadyPresent);

  const std::vector<Bookmark> list = store.List();
  ASSERT_EQ(list.size(), 1U);
  EXPECT_EQ(list[0].title, "First");
  EXPECT_EQ(list[0].created_at_ms, 1000);
}

TEST_F(BookmarksTest, RejectsUrlsThatAreNotWebPages) {
  BookmarkStore store = OpenStore();

  EXPECT_EQ(store.Add("javascript:alert(1)", "", 1),
            BookmarkAddResult::kNotWebUrl);
  EXPECT_EQ(store.Add("data:text/html,hi", "", 1),
            BookmarkAddResult::kNotWebUrl);
  EXPECT_EQ(store.Add("file:///etc/passwd", "", 1),
            BookmarkAddResult::kNotWebUrl);
  EXPECT_EQ(store.Add("", "", 1), BookmarkAddResult::kNotWebUrl);
  EXPECT_TRUE(store.List().empty());
}

TEST_F(BookmarksTest, RemoveDeletesOnlyThatUrl) {
  BookmarkStore store = OpenStore();
  ASSERT_EQ(store.Add("https://a.example/", "A", 1000),
            BookmarkAddResult::kAdded);
  ASSERT_EQ(store.Add("https://b.example/", "B", 2000),
            BookmarkAddResult::kAdded);

  EXPECT_TRUE(store.Remove("https://a.example/"));
  EXPECT_FALSE(store.Remove("https://a.example/"));
  EXPECT_FALSE(store.Remove("https://missing.example/"));

  const std::vector<Bookmark> list = store.List();
  ASSERT_EQ(list.size(), 1U);
  EXPECT_EQ(list[0].url, "https://b.example/");
  EXPECT_FALSE(store.Contains("https://a.example/"));
}

TEST_F(BookmarksTest, TextIsStoredAsData) {
  BookmarkStore store = OpenStore();
  const std::string url = "https://a.example/?q=');DROP TABLE bookmarks;--";
  ASSERT_EQ(store.Add(url, "it's \"quoted\"", 1), BookmarkAddResult::kAdded);

  const std::vector<Bookmark> list = store.List();

  ASSERT_EQ(list.size(), 1U);
  EXPECT_EQ(list[0].url, url);
  EXPECT_EQ(list[0].title, "it's \"quoted\"");
}

TEST_F(BookmarksTest, BookmarksSurviveReopeningTheFile) {
  {
    BookmarkStore store = OpenStore();
    ASSERT_EQ(store.Add("https://a.example/", "A", 1000),
              BookmarkAddResult::kAdded);
    ASSERT_EQ(store.Add("https://b.example/", "B", 2000),
              BookmarkAddResult::kAdded);
    ASSERT_TRUE(store.Remove("https://b.example/"));
  }

  BookmarkStore reopened = OpenStore();

  const std::vector<Bookmark> list = reopened.List();
  ASSERT_EQ(list.size(), 1U);
  EXPECT_EQ(list[0].url, "https://a.example/");
  EXPECT_EQ(list[0].title, "A");
}

TEST_F(BookmarksTest, SharesTheProfileDatabaseWithHistory) {
  std::string error;
  HistoryStore history = HistoryStore::Open(DatabasePath(), &error);
  ASSERT_TRUE(history.is_open()) << error;
  ASSERT_NE(history.AddVisit("https://a.example/", "A", 1000), 0);

  BookmarkStore bookmarks = OpenStore();
  ASSERT_EQ(bookmarks.Add("https://a.example/", "A", 2000),
            BookmarkAddResult::kAdded);

  EXPECT_EQ(history.Newest(10).size(), 1U);
  EXPECT_EQ(bookmarks.List().size(), 1U);
}

TEST_F(BookmarksTest, UpgradesAHistoryOnlyDatabase) {
  // A file written by the build that had only the history migration.
  {
    std::string error;
    Database db = Database::Open(DatabasePath(), &error);
    ASSERT_TRUE(db.is_open()) << error;
    const MigrationResult result =
        ApplyMigrations(db, ProfileMigrations().first(1));
    ASSERT_TRUE(result.ok) << result.error;
    ASSERT_EQ(result.version, 1);
  }

  BookmarkStore store = OpenStore();

  EXPECT_EQ(store.Add("https://a.example/", "A", 1), BookmarkAddResult::kAdded);
  std::string error;
  const Database db = Database::Open(DatabasePath(), &error);
  EXPECT_EQ(db.SchemaVersion(), static_cast<int>(ProfileMigrations().size()));
}

TEST_F(BookmarksTest, ClosedStoreFailsQuietly) {
  BookmarkStore store;

  EXPECT_FALSE(store.is_open());
  EXPECT_EQ(store.Add("https://a.example/", "A", 1),
            BookmarkAddResult::kFailed);
  EXPECT_FALSE(store.Remove("https://a.example/"));
  EXPECT_FALSE(store.Contains("https://a.example/"));
  EXPECT_TRUE(store.List().empty());
}

}  // namespace
}  // namespace vrowser
