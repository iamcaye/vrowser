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

TabList ThreeTabs() {
  TabList tabs;
  tabs.Append(1);
  tabs.Append(2);
  tabs.Append(3);
  return tabs;
}

TEST(TabListTest, NextAndPreviousWrap) {
  TabList tabs = ThreeTabs();  // 3 is active.
  EXPECT_EQ(tabs.ActivateNext(), 1);
  EXPECT_EQ(tabs.active(), 1);
  EXPECT_EQ(tabs.ActivatePrevious(), 3);
  EXPECT_EQ(tabs.ActivatePrevious(), 2);
  EXPECT_EQ(tabs.ActivateNext(), 3);
}

TEST(TabListTest, StepsMoveSeveralTabsAndWrap) {
  TabList tabs = ThreeTabs();
  tabs.Activate(1);
  EXPECT_EQ(tabs.ActivateNext(2), 3);
  EXPECT_EQ(tabs.ActivateNext(4), 1);
  EXPECT_EQ(tabs.ActivatePrevious(5), 2);
  // A count far larger than the list still lands on a tab.
  // From index 1, (1 + 99999) % 3 is index 1 again.
  EXPECT_EQ(tabs.ActivateNext(99999), 2);
}

TEST(TabListTest, StepsBelowOneMoveOne) {
  TabList tabs = ThreeTabs();
  tabs.Activate(2);
  EXPECT_EQ(tabs.ActivateNext(0), 3);
  EXPECT_EQ(tabs.ActivatePrevious(-7), 2);
}

TEST(TabListTest, CyclingOneTabStaysOnIt) {
  TabList tabs;
  tabs.Append(7);
  EXPECT_EQ(tabs.ActivateNext(), 7);
  EXPECT_EQ(tabs.ActivatePrevious(3), 7);
}

TEST(TabListTest, CyclingAnEmptyListDoesNothing) {
  TabList tabs;
  EXPECT_EQ(tabs.ActivateNext(), std::nullopt);
  EXPECT_EQ(tabs.ActivatePrevious(), std::nullopt);
  EXPECT_EQ(tabs.active(), std::nullopt);
}

}  // namespace
}  // namespace vrowser
