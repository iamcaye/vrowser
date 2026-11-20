// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/startup_config.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace vrowser {
namespace {

StartupConfig Parse(std::initializer_list<const char*> args) {
  std::vector<std::string> values;
  for (const char* arg : args) {
    values.emplace_back(arg);
  }
  return ParseStartupConfig(values);
}

TEST(StartupConfigTest, UsesDocumentedDefaults) {
  const StartupConfig config = Parse({});
  EXPECT_EQ(config.initial_url, "https://example.com");
  EXPECT_FALSE(config.user_data_dir.empty());
  EXPECT_FALSE(config.alloy_style);
  EXPECT_TRUE(config.use_views);
  EXPECT_FALSE(config.smoke_test);
}

TEST(StartupConfigTest, ReadsUrlEqualsForm) {
  const StartupConfig config = Parse({"--url=https://example.org/docs"});
  EXPECT_EQ(config.initial_url, "https://example.org/docs");
}

TEST(StartupConfigTest, ReadsUrlAsFollowingArgument) {
  const StartupConfig config = Parse({"--url", "https://example.org/path"});
  EXPECT_EQ(config.initial_url, "https://example.org/path");
}

TEST(StartupConfigTest, EmptyUrlKeepsTheDefault) {
  const StartupConfig config = Parse({"--url="});
  EXPECT_EQ(config.initial_url, "https://example.com");
}

TEST(StartupConfigTest, ReadsUserDataDirectory) {
  const StartupConfig config =
      Parse({"--user-data-dir", "/tmp/vrowser-profile"});
  EXPECT_EQ(config.user_data_dir,
            std::filesystem::path("/tmp/vrowser-profile"));
}

TEST(StartupConfigTest, SelectsAlloyStyleAndNativeWindow) {
  const StartupConfig config = Parse({"--use-alloy-style", "--use-native"});
  EXPECT_TRUE(config.alloy_style);
  EXPECT_FALSE(config.use_views);
}

TEST(StartupConfigTest, EnablesSmokeTest) {
  const StartupConfig config = Parse({"--vrowser-smoke-test"});
  EXPECT_TRUE(config.smoke_test);
}

TEST(StartupConfigTest, TabSmokeTestImpliesSmokeTest) {
  const StartupConfig config = Parse({"--vrowser-tab-smoke-test"});
  EXPECT_TRUE(config.tab_smoke_test);
  EXPECT_TRUE(config.smoke_test);
  EXPECT_FALSE(Parse({"--vrowser-smoke-test"}).tab_smoke_test);
}

TEST(StartupConfigTest, IgnoresCefSubprocessSwitches) {
  const StartupConfig config =
      Parse({"--type=renderer", "--lang=en-US", "--url=https://example.org"});
  EXPECT_EQ(config.initial_url, "https://example.org");
  EXPECT_FALSE(config.alloy_style);
}

TEST(StartupConfigTest, ArgvSkipsTheProgramName) {
  char program[] = "vrowser";
  char url[] = "--url=https://example.org";
  char* argv[] = {program, url};
  const StartupConfig config = ParseStartupConfig(2, argv);
  EXPECT_EQ(config.initial_url, "https://example.org");
}

}  // namespace
}  // namespace vrowser
