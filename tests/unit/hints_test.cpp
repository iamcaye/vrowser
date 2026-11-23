// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/hints.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace vrowser {
namespace {

std::vector<HintRect> Column(size_t count) {
  std::vector<HintRect> rects;
  for (size_t i = 0; i < count; ++i) {
    rects.push_back(
        {.x = 0, .y = static_cast<double>(i) * 20, .width = 50, .height = 10});
  }
  return rects;
}

bool PrefixFree(const std::vector<std::string>& labels) {
  for (size_t i = 0; i < labels.size(); ++i) {
    for (size_t j = 0; j < labels.size(); ++j) {
      if (i != j && labels[j].starts_with(labels[i])) {
        return false;
      }
    }
  }
  return true;
}

TEST(AssignHintLabelsTest, NoRectsNoLabels) {
  EXPECT_TRUE(AssignHintLabels({}).empty());
}

TEST(AssignHintLabelsTest, OneRectGetsOneCharacter) {
  EXPECT_EQ(AssignHintLabels(Column(1)), std::vector<std::string>{"a"});
}

TEST(AssignHintLabelsTest, FewRectsGetSingleCharactersInReadingOrder) {
  EXPECT_EQ(AssignHintLabels(Column(3)),
            (std::vector<std::string>{"a", "s", "d"}));
}

TEST(AssignHintLabelsTest, FullAlphabetStaysSingleCharacter) {
  const std::vector<std::string> labels = AssignHintLabels(Column(9));
  for (const std::string& label : labels) {
    EXPECT_EQ(label.size(), 1U);
  }
  EXPECT_EQ(std::set<std::string>(labels.begin(), labels.end()).size(), 9U);
}

TEST(AssignHintLabelsTest, MoreRectsMixLengthsAndStayPrefixFree) {
  const std::vector<std::string> labels = AssignHintLabels(Column(10));
  ASSERT_EQ(labels.size(), 10U);
  EXPECT_TRUE(PrefixFree(labels));
  // The first rect keeps a one-character label; only one was split.
  EXPECT_EQ(labels[0], "s");
  EXPECT_EQ(labels[7].size(), 1U);
  EXPECT_EQ(labels[8], "aa");
  EXPECT_EQ(labels[9], "as");
}

TEST(AssignHintLabelsTest, ManyRectsAreUniqueAndPrefixFree) {
  for (size_t count : {2U, 17U, 81U, 82U, 300U}) {
    const std::vector<std::string> labels = AssignHintLabels(Column(count));
    ASSERT_EQ(labels.size(), count);
    EXPECT_EQ(std::set<std::string>(labels.begin(), labels.end()).size(),
              count);
    EXPECT_TRUE(PrefixFree(labels)) << count;
  }
}

TEST(AssignHintLabelsTest, LabelsAreAsShortAsTheCountAllows) {
  // 81 = 9 * 9 fits in two characters; 82 needs some three.
  for (const std::string& label : AssignHintLabels(Column(81))) {
    EXPECT_LE(label.size(), 2U);
  }
  size_t three = 0;
  for (const std::string& label : AssignHintLabels(Column(82))) {
    EXPECT_LE(label.size(), 3U);
    three += label.size() == 3 ? 1 : 0;
  }
  EXPECT_GT(three, 0U);
}

TEST(AssignHintLabelsTest, ReadingOrderIsTopThenLeft) {
  const std::vector<HintRect> rects = {
      {.x = 200, .y = 100, .width = 10, .height = 10},
      {.x = 0, .y = 100, .width = 10, .height = 10},
      {.x = 500, .y = 0, .width = 10, .height = 10},
  };
  EXPECT_EQ(AssignHintLabels(rects), (std::vector<std::string>{"d", "s", "a"}));
}

TEST(AssignHintLabelsTest, CustomAlphabet) {
  EXPECT_EQ(AssignHintLabels(Column(3), "ab"),
            (std::vector<std::string>{"b", "aa", "ab"}));
}

TEST(AssignHintLabelsTest, UnusableAlphabetGivesEmptyLabels) {
  EXPECT_EQ(AssignHintLabels(Column(2), "a"),
            (std::vector<std::string>{"", ""}));
  EXPECT_EQ(AssignHintLabels(Column(2), "aa"),
            (std::vector<std::string>{"", ""}));
}

TEST(HintSessionTest, SingleCharacterMatches) {
  HintSession session({"a", "s", "d"});
  const HintStep step = session.Type("s");
  EXPECT_EQ(step.status, HintStatus::kMatched);
  EXPECT_EQ(step.index, 1U);
}

TEST(HintSessionTest, PrefixWaitsForTheRest) {
  HintSession session({"s", "aa", "as"});
  EXPECT_EQ(session.Type("a").status, HintStatus::kPending);
  EXPECT_EQ(session.typed(), "a");
  const HintStep step = session.Type("s");
  EXPECT_EQ(step.status, HintStatus::kMatched);
  EXPECT_EQ(step.index, 2U);
}

TEST(HintSessionTest, UppercaseMatchesLowercaseLabels) {
  HintSession session({"a", "s"});
  EXPECT_EQ(session.Type("S").index, 1U);
}

TEST(HintSessionTest, UnknownKeyIsDroppedAndKeepsTheTypedText) {
  HintSession session({"s", "aa", "as"});
  ASSERT_EQ(session.Type("a").status, HintStatus::kPending);
  EXPECT_EQ(session.Type("z").status, HintStatus::kNoMatch);
  EXPECT_EQ(session.Type("Enter").status, HintStatus::kNoMatch);
  EXPECT_EQ(session.typed(), "a");
  EXPECT_EQ(session.Type("a").index, 1U);
}

TEST(HintSessionTest, BackspaceUndoesOneCharacter) {
  HintSession session({"s", "aa", "as"});
  ASSERT_EQ(session.Type("a").status, HintStatus::kPending);
  EXPECT_EQ(session.Type("Backspace").status, HintStatus::kPending);
  EXPECT_EQ(session.typed(), "");
  EXPECT_EQ(session.Type("Backspace").status, HintStatus::kPending);
  EXPECT_EQ(session.Type("s").index, 0U);
}

}  // namespace
}  // namespace vrowser
