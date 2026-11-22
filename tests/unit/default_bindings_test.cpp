// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/default_bindings.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {
namespace {

class DefaultBindingsTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(AddDefaultBindings(bindings_)); }

  // Feeds each notation token in Normal mode and returns the last result.
  KeyResolution Type(const std::vector<std::string>& keys, Mode mode) {
    KeySequenceState state;
    KeyResolution result;
    for (const std::string& key : keys) {
      result = bindings_.Resolve(mode, state, key);
    }
    return result;
  }

  KeyResolution Normal(const std::vector<std::string>& keys) {
    return Type(keys, Mode::kNormal);
  }

  KeyBindings bindings_;
};

TEST_F(DefaultBindingsTest, EachKeyRunsItsCommand) {
  const struct {
    std::vector<std::string> keys;
    const char* command;
  } kCases[] = {
      {{"r"}, "reload"},
      {{"j"}, "scroll-down"},
      {{"k"}, "scroll-up"},
      {{"d"}, "scroll-half-page-down"},
      {{"u"}, "scroll-half-page-up"},
      {{"g", "g"}, "scroll-to-top"},
      {{"G"}, "scroll-to-bottom"},
      {{"g", "t"}, "tab-next"},
      {{"g", "T"}, "tab-previous"},
      {{"t"}, "tab-new"},
      {{"x"}, "tab-close"},
  };
  for (const auto& c : kCases) {
    const KeyResolution result = Normal(c.keys);
    EXPECT_EQ(result.status, KeyStatus::kMatched) << c.command;
    EXPECT_EQ(result.action.command, c.command);
    EXPECT_TRUE(result.action.args.empty()) << c.command;
    EXPECT_FALSE(result.has_count) << c.command;
    EXPECT_EQ(result.count, 1) << c.command;
  }
}

TEST_F(DefaultBindingsTest, CountIsPassedWithTheMotion) {
  const KeyResolution down = Normal({"5", "j"});
  EXPECT_EQ(down.action.command, "scroll-down");
  EXPECT_TRUE(down.has_count);
  EXPECT_EQ(down.count, 5);

  const KeyResolution up = Normal({"1", "2", "k"});
  EXPECT_EQ(up.action.command, "scroll-up");
  EXPECT_EQ(up.count, 12);
}

TEST_F(DefaultBindingsTest, CountIsPassedWithTabMotions) {
  const KeyResolution next = Normal({"3", "g", "t"});
  EXPECT_EQ(next.action.command, "tab-next");
  EXPECT_TRUE(next.has_count);
  EXPECT_EQ(next.count, 3);
}

TEST_F(DefaultBindingsTest, SingleGIsAPrefix) {
  EXPECT_EQ(Normal({"g"}).status, KeyStatus::kPrefix);
  EXPECT_EQ(Normal({"g", "j"}).status, KeyStatus::kNoMatch);
}

TEST_F(DefaultBindingsTest, OnlyNormalModeHasBindings) {
  EXPECT_EQ(Type({"j"}, Mode::kInsert).status, KeyStatus::kNoMatch);
  EXPECT_EQ(Type({"G"}, Mode::kInsert).status, KeyStatus::kNoMatch);
}

TEST_F(DefaultBindingsTest, AddingTwiceFails) {
  EXPECT_FALSE(AddDefaultBindings(bindings_));
}

}  // namespace
}  // namespace vrowser
