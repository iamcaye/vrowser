// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/navigation.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace vrowser {
namespace {

TEST(NavigationControllerTest, StartsEmpty) {
  const NavigationController navigation;
  EXPECT_TRUE(navigation.requested_url().empty());
  EXPECT_EQ(navigation.sequence(), 0u);
}

TEST(NavigationControllerTest, RequestIsObservedWithoutCef) {
  NavigationController navigation;
  std::vector<std::string> loads;
  navigation.SetListener(
      [&loads](const std::string& url) { loads.push_back(url); });

  EXPECT_TRUE(navigation.Request("https://neovim.io/"));
  EXPECT_EQ(navigation.requested_url(), "https://neovim.io/");
  EXPECT_EQ(navigation.sequence(), 1u);
  EXPECT_EQ(loads, std::vector<std::string>{"https://neovim.io/"});
}

TEST(NavigationControllerTest, RepeatedRequestIsANewChange) {
  NavigationController navigation;
  int calls = 0;
  navigation.SetListener([&calls](const std::string&) { ++calls; });

  navigation.Request("https://example.com/");
  navigation.Request("https://example.com/");
  EXPECT_EQ(navigation.sequence(), 2u);
  EXPECT_EQ(calls, 2);
}

TEST(NavigationControllerTest, TrimsWhitespace) {
  NavigationController navigation;
  EXPECT_TRUE(navigation.Request("  https://example.com/\n"));
  EXPECT_EQ(navigation.requested_url(), "https://example.com/");
}

TEST(NavigationControllerTest, RejectsEmptyAndJavaScriptUrls) {
  NavigationController navigation;
  int calls = 0;
  navigation.SetListener([&calls](const std::string&) { ++calls; });
  ASSERT_TRUE(navigation.Request("https://example.com/"));

  EXPECT_FALSE(navigation.Request(""));
  EXPECT_FALSE(navigation.Request("   "));
  EXPECT_FALSE(navigation.Request("javascript:alert(1)"));
  EXPECT_FALSE(navigation.Request(" JavaScript:alert(1)"));

  EXPECT_EQ(navigation.requested_url(), "https://example.com/");
  EXPECT_EQ(navigation.sequence(), 1u);
  EXPECT_EQ(calls, 1);
}

TEST(NavigationControllerTest, WorksWithoutListener) {
  NavigationController navigation;
  EXPECT_TRUE(navigation.Request("about:blank"));
  EXPECT_EQ(navigation.requested_url(), "about:blank");
}

}  // namespace
}  // namespace vrowser
