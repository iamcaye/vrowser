// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/profile.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>

namespace vrowser {
namespace {

namespace fs = std::filesystem;

TEST(ProfileTest, AcceptsSimpleNames) {
  EXPECT_TRUE(IsValidProfileName("default"));
  EXPECT_TRUE(IsValidProfileName("work"));
  EXPECT_TRUE(IsValidProfileName("client-2_test"));
  EXPECT_TRUE(IsValidProfileName("0"));
  EXPECT_TRUE(IsValidProfileName(std::string(64, 'a')));
}

TEST(ProfileTest, RejectsNamesThatCouldLeaveTheProfilesDirectory) {
  EXPECT_FALSE(IsValidProfileName(""));
  EXPECT_FALSE(IsValidProfileName("."));
  EXPECT_FALSE(IsValidProfileName(".."));
  EXPECT_FALSE(IsValidProfileName("../other"));
  EXPECT_FALSE(IsValidProfileName("a/b"));
  EXPECT_FALSE(IsValidProfileName("a\\b"));
  EXPECT_FALSE(IsValidProfileName("/abs"));
  EXPECT_FALSE(IsValidProfileName("c:"));
}

TEST(ProfileTest, RejectsOtherCharactersAndLengths) {
  EXPECT_FALSE(IsValidProfileName("Work"));
  EXPECT_FALSE(IsValidProfileName("my profile"));
  EXPECT_FALSE(IsValidProfileName("-flag"));
  EXPECT_FALSE(IsValidProfileName("caf\xC3\xA9"));
  EXPECT_FALSE(IsValidProfileName(std::string("a\0b", 3)));
  EXPECT_FALSE(IsValidProfileName(std::string(65, 'a')));
}

TEST(ProfileTest, DirectoryIsAChildOfTheRoot) {
  const fs::path root = fs::path("/data") / "vrowser";
  EXPECT_EQ(ProfileDirectory(root, "work"), root / "Profile-work");
  EXPECT_EQ(ProfileDirectory(root, std::string(kDefaultProfileName)),
            root / "Default");
}

TEST(ProfileTest, TwoProfilesGetDifferentDirectories) {
  const fs::path root = "/data";
  const std::optional<fs::path> a = ProfileDirectory(root, "a");
  const std::optional<fs::path> b = ProfileDirectory(root, "b");
  ASSERT_TRUE(a && b);
  EXPECT_NE(*a, *b);
  EXPECT_EQ(a->parent_path(), b->parent_path());
}

TEST(ProfileTest, InvalidNameHasNoDirectory) {
  EXPECT_FALSE(ProfileDirectory("/data", "..").has_value());
  EXPECT_FALSE(ProfileDirectory("/data", "a/b").has_value());
}

}  // namespace
}  // namespace vrowser
