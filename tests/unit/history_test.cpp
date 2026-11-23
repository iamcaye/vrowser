// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/storage/history.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace vrowser {
namespace {

namespace fs = std::filesystem;

constexpr int kTab = 7;
constexpr int kOtherTab = 8;

class HistoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<int> counter{0};
    dir_ = fs::temp_directory_path() /
           ("vrowser-history-" +
            std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "-" + std::to_string(counter++));
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  [[nodiscard]] HistoryStore OpenStore() const {
    std::string error;
    HistoryStore store = HistoryStore::Open(
        dir_ / std::string(kProfileDatabaseFileName), &error);
    EXPECT_TRUE(store.is_open()) << error;
    return store;
  }

 private:
  fs::path dir_;
};

TEST_F(HistoryTest, NewestReturnsVisitsNewestFirst) {
  HistoryStore store = OpenStore();
  ASSERT_NE(store.AddVisit("https://a.example/", "A", 1000), 0);
  ASSERT_NE(store.AddVisit("https://b.example/", "B", 3000), 0);
  ASSERT_NE(store.AddVisit("https://c.example/", "C", 2000), 0);

  const std::vector<HistoryEntry> newest = store.Newest(2);

  ASSERT_EQ(newest.size(), 2U);
  EXPECT_EQ(newest[0].url, "https://b.example/");
  EXPECT_EQ(newest[1].url, "https://c.example/");
  EXPECT_EQ(newest[1].title, "C");
  EXPECT_EQ(newest[1].visited_at_ms, 2000);
}

TEST_F(HistoryTest, MainFrameCommitInsertsOneRow) {
  HistoryRecorder recorder(OpenStore());

  EXPECT_TRUE(recorder.OnCommit(kTab, true, "https://example.com/", 1000));

  const std::vector<HistoryEntry> newest = recorder.store().Newest(10);
  ASSERT_EQ(newest.size(), 1U);
  EXPECT_EQ(newest[0].url, "https://example.com/");
  EXPECT_EQ(newest[0].visited_at_ms, 1000);
}

TEST_F(HistoryTest, SubframeLoadsDoNotInsertRows) {
  HistoryRecorder recorder(OpenStore());

  EXPECT_FALSE(recorder.OnCommit(kTab, false, "https://ads.example/", 1000));

  EXPECT_TRUE(recorder.store().Newest(10).empty());
}

TEST_F(HistoryTest, NonWebPagesAreNotRecorded) {
  HistoryRecorder recorder(OpenStore());

  for (const char* url : {
           "data:text/html,error",
           "about:blank",
           "chrome://version/",
           "file:///etc/passwd",
           "javascript:alert(1)",
           "",
       }) {
    EXPECT_FALSE(recorder.OnCommit(kTab, true, url, 1000)) << url;
  }
  EXPECT_TRUE(recorder.OnCommit(kTab, true, "HTTP://EXAMPLE.COM/", 1000));

  EXPECT_EQ(recorder.store().Newest(10).size(), 1U);
}

TEST_F(HistoryTest, TitleNamesTheTabsLatestVisit) {
  HistoryRecorder recorder(OpenStore());
  ASSERT_TRUE(recorder.OnCommit(kTab, true, "https://a.example/", 1000));
  ASSERT_TRUE(recorder.OnCommit(kOtherTab, true, "https://b.example/", 2000));

  recorder.OnTitle(kTab, "Page A");
  recorder.OnTitle(kOtherTab, "Page B");

  const std::vector<HistoryEntry> newest = recorder.store().Newest(10);
  ASSERT_EQ(newest.size(), 2U);
  EXPECT_EQ(newest[0].title, "Page B");
  EXPECT_EQ(newest[1].title, "Page A");
}

TEST_F(HistoryTest, TitleAfterANonWebPageDoesNotRenameTheOldVisit) {
  HistoryRecorder recorder(OpenStore());
  ASSERT_TRUE(recorder.OnCommit(kTab, true, "https://a.example/", 1000));
  ASSERT_FALSE(recorder.OnCommit(kTab, true, "data:text/html,error", 2000));

  recorder.OnTitle(kTab, "Error page");

  EXPECT_EQ(recorder.store().Newest(1)[0].title, "");
}

TEST_F(HistoryTest, ClosedTabNoLongerTakesTitles) {
  HistoryRecorder recorder(OpenStore());
  ASSERT_TRUE(recorder.OnCommit(kTab, true, "https://a.example/", 1000));

  recorder.OnTabClosed(kTab);
  recorder.OnTitle(kTab, "Late title");

  EXPECT_EQ(recorder.store().Newest(1)[0].title, "");
}

TEST_F(HistoryTest, ReopeningTheDatabaseReturnsTheRow) {
  {
    HistoryRecorder recorder(OpenStore());
    ASSERT_TRUE(recorder.OnCommit(kTab, true, "https://example.com/", 1000));
    recorder.OnTitle(kTab, "Example Domain");
  }

  const std::vector<HistoryEntry> newest = OpenStore().Newest(10);

  ASSERT_EQ(newest.size(), 1U);
  EXPECT_EQ(newest[0].url, "https://example.com/");
  EXPECT_EQ(newest[0].title, "Example Domain");
}

TEST_F(HistoryTest, ClosedStoreWritesNothing) {
  HistoryStore store;
  EXPECT_FALSE(store.is_open());
  EXPECT_EQ(store.AddVisit("https://example.com/", "", 1), 0);
  EXPECT_FALSE(store.SetTitle(1, "x"));
  EXPECT_TRUE(store.Newest(10).empty());
}

}  // namespace
}  // namespace vrowser
