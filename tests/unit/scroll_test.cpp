// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/scroll.hpp"

#include <gtest/gtest.h>

#include <string>

namespace vrowser {
namespace {

TEST(ScrollScriptTest, LineStepsMultiplyByCount) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 1),
            "window.scrollBy({top: 60, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 3),
            "window.scrollBy({top: 180, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineUp, 2),
            "window.scrollBy({top: -120, behavior: 'instant'});");
}

TEST(ScrollScriptTest, CountBelowOneIsOne) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 0),
            ScrollScript(ScrollMotion::kLineDown, 1));
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineUp, -4),
            ScrollScript(ScrollMotion::kLineUp, 1));
}

TEST(ScrollScriptTest, LargestCountDoesNotOverflow) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 99999),
            "window.scrollBy({top: 5999940, behavior: 'instant'});");
}

TEST(ScrollScriptTest, HalfPageUsesViewportHeight) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kHalfPageDown, 1),
            "window.scrollBy({top: window.innerHeight / 2 * 1, "
            "behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kHalfPageUp, 4),
            "window.scrollBy({top: -window.innerHeight / 2 * 4, "
            "behavior: 'instant'});");
}

TEST(ScrollScriptTest, TopAndBottomIgnoreCount) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kTop, 7),
            "window.scrollTo({top: 0, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kBottom, 7),
            ScrollScript(ScrollMotion::kBottom, 1));
  EXPECT_NE(ScrollScript(ScrollMotion::kBottom, 1).find("scrollHeight"),
            std::string::npos);
}

}  // namespace
}  // namespace vrowser
