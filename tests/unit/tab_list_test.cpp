// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/tab_list.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace vrowser {
namespace {

using Ids = std::vector<TabId>;

TEST(TabListTest, StartsEmpty) {
  const TabList tabs;
  EXPECT_TRUE(tabs.empty());
  EXPECT_EQ(tabs.size(), 0u);
  EXPECT_EQ(tabs.active(), std::nullopt);
  EXPECT_FALSE(tabs.Contains(1));
}

TEST(TabListTest, AppendKeepsOrderAndActivatesNewTab) {
  TabList tabs;
  EXPECT_TRUE(tabs.Append(1));
  EXPECT_EQ(tabs.active(), 1);
  EXPECT_TRUE(tabs.Append(2));
  EXPECT_TRUE(tabs.Append(3));
  EXPECT_EQ(tabs.tabs(), (Ids{1, 2, 3}));
  EXPECT_EQ(tabs.active(), 3);
}

TEST(TabListTest, AppendRejectsDuplicates) {
  TabList tabs;
  tabs.Append(1);
  tabs.Append(2);
  EXPECT_FALSE(tabs.Append(1));
  EXPECT_EQ(tabs.tabs(), (Ids{1, 2}));
  EXPECT_EQ(tabs.active(), 2);
}

TEST(TabListTest, CloseMiddleActiveTabActivatesNextTab) {
  TabList tabs;
  tabs.Append(1);
  tabs.Append(2);
  tabs.Append(3);
  ASSERT_TRUE(tabs.Activate(2));

  EXPECT_TRUE(tabs.Close(2));
  EXPECT_EQ(tabs.tabs(), (Ids{1, 3}));
  EXPECT_EQ(tabs.active(), 3);
}

TEST(TabListTest, CloseInactiveTabKeepsActiveTab) {
  TabList tabs;
  tabs.Append(1);
  tabs.Append(2);
  tabs.Append(3);

  EXPECT_TRUE(tabs.Close(2));
  EXPECT_EQ(tabs.tabs(), (Ids{1, 3}));
  EXPECT_EQ(tabs.active(), 3);
}

TEST(TabListTest, CloseLastPositionActivatesPreviousTab) {
  TabList tabs;
  tabs.Append(1);
  tabs.Append(2);

  EXPECT_TRUE(tabs.Close(2));
  EXPECT_EQ(tabs.tabs(), Ids{1});
  EXPECT_EQ(tabs.active(), 1);
}

TEST(TabListTest, CloseLastRemainingTabEmptiesList) {
  TabList tabs;
  tabs.Append(7);

  EXPECT_TRUE(tabs.Close(7));
  EXPECT_TRUE(tabs.empty());
  EXPECT_EQ(tabs.active(), std::nullopt);
}

TEST(TabListTest, CloseAndActivateRejectUnknownIds) {
  TabList tabs;
  tabs.Append(1);
  EXPECT_FALSE(tabs.Close(9));
  EXPECT_FALSE(tabs.Activate(9));
  EXPECT_EQ(tabs.tabs(), Ids{1});
  EXPECT_EQ(tabs.active(), 1);
}

}  // namespace
}  // namespace vrowser
