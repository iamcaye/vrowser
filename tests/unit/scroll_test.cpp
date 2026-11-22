// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/scroll.hpp"

#include <gtest/gtest.h>

#include <string>

namespace vrowser {
namespace {

TEST(ScrollScriptTest, LineStepsMultiplyByCount) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 1, 0),
            "window.scrollBy({top: 60, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 3, 0),
            "window.scrollBy({top: 180, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineUp, 2, 0),
            "window.scrollBy({top: -120, behavior: 'instant'});");
}

TEST(ScrollScriptTest, CountBelowOneIsOne) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 0, 0),
            ScrollScript(ScrollMotion::kLineDown, 1, 0));
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineUp, -4, 0),
            ScrollScript(ScrollMotion::kLineUp, 1, 0));
}

TEST(ScrollScriptTest, LargestCountDoesNotOverflow) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 99999, 0),
            "window.scrollBy({top: 5999940, behavior: 'instant'});");
}

TEST(ScrollScriptTest, HalfPageUsesViewportHeight) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kHalfPageDown, 1, 0),
            "window.scrollBy({top: window.innerHeight / 2 * 1, "
            "behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kHalfPageUp, 4, 0),
            "window.scrollBy({top: -window.innerHeight / 2 * 4, "
            "behavior: 'instant'});");
}

TEST(ScrollScriptTest, TopAndBottomIgnoreCount) {
  EXPECT_EQ(ScrollScript(ScrollMotion::kTop, 7, 0),
            "window.scrollTo({top: 0, behavior: 'instant'});");
  EXPECT_EQ(ScrollScript(ScrollMotion::kBottom, 7, 0),
            ScrollScript(ScrollMotion::kBottom, 1, 0));
  EXPECT_NE(ScrollScript(ScrollMotion::kBottom, 1, 0).find("scrollHeight"),
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
            "(() => { const s = window.__vrowserScroll; if (s) { s.until "
            "= performance.now() + 600; } })();");
  EXPECT_EQ(HeldScrollStopScript(),
            "(() => { const s = window.__vrowserScroll; if (s) { ++s.id; "
            "s.animating = false; } })();");
}

TEST(AnimatedScrollTest, DefaultIsAnimated) {
  EXPECT_GT(kScrollAnimationMs, 0);
  EXPECT_EQ(ScrollScript(ScrollMotion::kLineDown, 1),
            ScrollScript(ScrollMotion::kLineDown, 1, kScrollAnimationMs));
  EXPECT_NE(ScrollScript(ScrollMotion::kLineDown, 1),
            ScrollScript(ScrollMotion::kLineDown, 1, 0));
}

TEST(AnimatedScrollTest, StepsAddToThePendingTarget) {
  const std::string down = ScrollScript(ScrollMotion::kLineDown, 3, 120);
  EXPECT_NE(down.find("const base = s.animating ? s.target : window.scrollY;"),
            std::string::npos);
  EXPECT_NE(down.find("Math.max(0, base + 180)"), std::string::npos);
  EXPECT_NE(down.find("/ 120"), std::string::npos);
  EXPECT_NE(ScrollScript(ScrollMotion::kLineUp, 2, 120)
                .find("Math.max(0, base - 120)"),
            std::string::npos);
  EXPECT_NE(ScrollScript(ScrollMotion::kHalfPageUp, 4, 120)
                .find("base - window.innerHeight / 2 * 4"),
            std::string::npos);
}

TEST(AnimatedScrollTest, TargetIsClampedToThePage) {
  const std::string script = ScrollScript(ScrollMotion::kLineDown, 1, 120);
  EXPECT_NE(script.find("Math.min(max, Math.max(0, "), std::string::npos);
  EXPECT_NE(ScrollScript(ScrollMotion::kTop, 9, 120)
                .find("Math.min(max, Math.max(0, 0))"),
            std::string::npos);
  EXPECT_NE(ScrollScript(ScrollMotion::kBottom, 9, 120)
                .find("Math.min(max, Math.max(0, max))"),
            std::string::npos);
}

TEST(AnimatedScrollTest, ReducedMotionScrollsInstantly) {
  const std::string script = ScrollScript(ScrollMotion::kHalfPageDown, 1, 120);
  EXPECT_NE(script.find("prefers-reduced-motion: reduce"), std::string::npos);
  EXPECT_NE(script.find("window.scrollTo({top: target, behavior: 'instant'}); "
                        "return;"),
            std::string::npos);
}

TEST(AnimatedScrollTest, HoldAndStepShareTheLoopState) {
  const std::string step = ScrollScript(ScrollMotion::kLineDown, 1, 120);
  const std::string hold = HeldScrollStartScript(ScrollMotion::kLineDown);
  EXPECT_NE(step.find("window.__vrowserScroll"), std::string::npos);
  EXPECT_NE(hold.find("window.__vrowserScroll"), std::string::npos);
  // Starting a hold cancels a step animation.
  EXPECT_NE(hold.find("const id = ++s.id; s.animating = false;"),
            std::string::npos);
}

}  // namespace
}  // namespace vrowser
