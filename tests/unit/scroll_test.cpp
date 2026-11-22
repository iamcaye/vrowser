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

TEST(HeldScrollTest, OnlyLineAndHalfPageAreContinuous) {
  EXPECT_TRUE(IsContinuousMotion(ScrollMotion::kLineDown));
  EXPECT_TRUE(IsContinuousMotion(ScrollMotion::kLineUp));
  EXPECT_TRUE(IsContinuousMotion(ScrollMotion::kHalfPageDown));
  EXPECT_TRUE(IsContinuousMotion(ScrollMotion::kHalfPageUp));
  EXPECT_FALSE(IsContinuousMotion(ScrollMotion::kTop));
  EXPECT_FALSE(IsContinuousMotion(ScrollMotion::kBottom));
  EXPECT_TRUE(HeldScrollStartScript(ScrollMotion::kTop).empty());
  EXPECT_TRUE(HeldScrollStartScript(ScrollMotion::kBottom).empty());
}

TEST(HeldScrollTest, StartScriptCarriesDirectionAndDeadline) {
  const std::string down = HeldScrollStartScript(ScrollMotion::kLineDown);
  EXPECT_NE(down.find("const v = 900;"), std::string::npos);
  EXPECT_NE(down.find("performance.now() + 600"), std::string::npos);
  EXPECT_NE(down.find("requestAnimationFrame"), std::string::npos);
  EXPECT_NE(
      HeldScrollStartScript(ScrollMotion::kLineUp).find("const v = -900;"),
      std::string::npos);
  EXPECT_NE(HeldScrollStartScript(ScrollMotion::kHalfPageUp)
                .find("const v = -window.innerHeight * 2;"),
            std::string::npos);
}

TEST(HeldScrollTest, KeepAliveAndStopTouchOnlyTheLoopState) {
  EXPECT_EQ(HeldScrollKeepAliveScript(),
            "(() => { const s = window.__vrowserHeldScroll; if (s) { s.until "
            "= performance.now() + 600; } })();");
  EXPECT_EQ(HeldScrollStopScript(),
            "(() => { const s = window.__vrowserHeldScroll; if (s) { ++s.id; "
            "} })();");
}

}  // namespace
}  // namespace vrowser
