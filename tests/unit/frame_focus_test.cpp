// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/frame_focus.hpp"

#include <gtest/gtest.h>

#include <string>

namespace vrowser {
namespace {

TEST(FrameFocusTest, StartsNotEditable) {
  const FrameFocus focus;
  EXPECT_FALSE(focus.Editable("main"));
  EXPECT_FALSE(focus.Editable(""));
}

TEST(FrameFocusTest, FocusedFrameDecides) {
  FrameFocus focus;
  focus.Report("main", true);
  EXPECT_TRUE(focus.Editable("main"));
  focus.Report("main", false);
  EXPECT_FALSE(focus.Editable("main"));
}

TEST(FrameFocusTest, ChildFrameInputIsEditable) {
  FrameFocus focus;
  focus.Report("main", false);
  focus.Report("child", true);
  EXPECT_TRUE(focus.Editable("child"));
  EXPECT_FALSE(focus.Editable("main"));
}

// Frames in different renderer processes report in any order. The parent's
// "focus left my input" can arrive after the child's "input focused".
TEST(FrameFocusTest, ReportsFromOtherFramesArriveInAnyOrder) {
  FrameFocus focus;
  focus.Report("main", true);
  focus.Report("child", true);
  focus.Report("main", false);
  EXPECT_TRUE(focus.Editable("child"));
}

// A frame that loses focus to another frame may never report a blur. Its
// stale input must not keep the tab in Insert.
TEST(FrameFocusTest, StaleChildReportIsIgnoredWhenAnotherFrameHasFocus) {
  FrameFocus focus;
  focus.Report("child", true);
  focus.Report("main", false);
  EXPECT_FALSE(focus.Editable("main"));
}

TEST(FrameFocusTest, UnreportedFocusedFrameIsNotEditable) {
  FrameFocus focus;
  focus.Report("child", true);
  EXPECT_FALSE(focus.Editable("other"));
}

TEST(FrameFocusTest, UnknownFocusedFrameUsesTheLatestReport) {
  FrameFocus focus;
  focus.Report("main", false);
  focus.Report("child", true);
  EXPECT_TRUE(focus.Editable(""));
  focus.Report("main", false);
  EXPECT_FALSE(focus.Editable(""));
}

TEST(FrameFocusTest, ClearForgetsEveryFrame) {
  FrameFocus focus;
  focus.Report("main", true);
  focus.Report("child", true);
  focus.Clear();
  EXPECT_FALSE(focus.Editable("main"));
  EXPECT_FALSE(focus.Editable("child"));
  EXPECT_FALSE(focus.Editable(""));
}

TEST(FrameFocusTest, ReportedFramesAreBounded) {
  FrameFocus focus;
  for (int i = 0; i < 1000; ++i) {
    focus.Report("frame" + std::to_string(i), true);
  }
  EXPECT_LE(focus.size(), FrameFocus::kMaxFrames);
  // The most recent reports are kept.
  EXPECT_TRUE(focus.Editable("frame999"));
}

}  // namespace
}  // namespace vrowser
