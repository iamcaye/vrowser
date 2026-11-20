// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_dispatcher.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace vrowser {
namespace {

KeyEvent Down(std::string key, char32_t character = 0, Modifiers mods = {}) {
  return KeyEvent{std::move(key), character, mods, true};
}

KeyEvent Up(std::string key) {
  return KeyEvent{std::move(key), 0, Modifiers{}, false};
}

class KeyDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "r", "reload"));
    ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "gg", "scroll-top"));
  }

  KeyDispatch Send(const KeyEvent& event) {
    return DispatchKey(bindings_, state_, event);
  }

  KeyBindings bindings_;
  KeyInputState state_;
};

TEST_F(KeyDispatcherTest, NormalBindingIsConsumedAndRuns) {
  const KeyDispatch dispatch = Send(Down("r", U'r'));
  EXPECT_EQ(dispatch.disposition, KeyDisposition::kConsume);
  ASSERT_TRUE(dispatch.action.has_value());
  EXPECT_EQ(dispatch.action->command, "reload");
  EXPECT_EQ(dispatch.count, 1);
}

TEST_F(KeyDispatcherTest, KeyUpOfConsumedKeyIsConsumed) {
  Send(Down("r", U'r'));
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kConsume);
  // Only once: a later stray key-up goes to the page.
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, UnboundNormalKeyIsForwarded) {
  EXPECT_EQ(Send(Down("x", U'x')).disposition, KeyDisposition::kForward);
  EXPECT_EQ(Send(Up("x")).disposition, KeyDisposition::kForward);
  // Shortcuts such as Command+L are not bound and reach Chromium.
  const KeyDispatch shortcut = Send(Down("l", U'l', {.meta = true}));
  EXPECT_EQ(shortcut.disposition, KeyDisposition::kForward);
  EXPECT_FALSE(shortcut.action.has_value());
}

TEST_F(KeyDispatcherTest, PrefixIsConsumedThenResolves) {
  EXPECT_EQ(Send(Down("g", U'g')).disposition, KeyDisposition::kConsume);
  const KeyDispatch dispatch = Send(Down("g", U'g'));
  EXPECT_EQ(dispatch.disposition, KeyDisposition::kConsume);
  ASSERT_TRUE(dispatch.action.has_value());
  EXPECT_EQ(dispatch.action->command, "scroll-top");
}

TEST_F(KeyDispatcherTest, AbandonedPrefixIsConsumedAndCleared) {
  Send(Down("g", U'g'));
  const KeyDispatch dispatch = Send(Down("x", U'x'));
  EXPECT_EQ(dispatch.disposition, KeyDisposition::kConsume);
  EXPECT_FALSE(dispatch.action.has_value());
  EXPECT_TRUE(state_.sequence.keys.empty());
  EXPECT_EQ(Send(Down("x", U'x')).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, CountIsConsumedAndPassedToAction) {
  EXPECT_EQ(Send(Down("3", U'3')).disposition, KeyDisposition::kConsume);
  const KeyDispatch dispatch = Send(Down("r", U'r'));
  EXPECT_EQ(dispatch.count, 3);
  EXPECT_TRUE(dispatch.has_count);
}

TEST_F(KeyDispatcherTest, EscapeInNormalClearsPendingOrForwards) {
  EXPECT_EQ(Send(Down("Escape")).disposition, KeyDisposition::kForward);
  Send(Down("g", U'g'));
  EXPECT_EQ(Send(Down("Escape")).disposition, KeyDisposition::kConsume);
  EXPECT_TRUE(state_.sequence.keys.empty());
}

TEST_F(KeyDispatcherTest, EditableFocusStartsInsertAndForwardsKeys) {
  state_.mode.Apply(ModeEvent::kEditableFocusGained);
  ASSERT_EQ(state_.mode.mode(), Mode::kInsert);

  const KeyDispatch dispatch = Send(Down("r", U'r'));
  EXPECT_EQ(dispatch.disposition, KeyDisposition::kForward);
  EXPECT_FALSE(dispatch.action.has_value());
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, EscapeLeavesInsertAndIsConsumed) {
  state_.mode.Apply(ModeEvent::kEditableFocusGained);

  EXPECT_EQ(Send(Down("Escape")).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(state_.mode.mode(), Mode::kNormal);
  EXPECT_EQ(Send(Up("Escape")).disposition, KeyDisposition::kConsume);

  // Back in Normal with the field still focused: bindings work again.
  EXPECT_EQ(Send(Down("r", U'r')).action->command, "reload");
}

TEST_F(KeyDispatcherTest, ModifiedEscapeInInsertIsForwarded) {
  state_.mode.Apply(ModeEvent::kEditableFocusGained);
  EXPECT_EQ(Send(Down("Escape", 0, {.shift = true})).disposition,
            KeyDisposition::kForward);
  EXPECT_EQ(state_.mode.mode(), Mode::kInsert);
}

TEST_F(KeyDispatcherTest, CommandAndHintOnlyHandleEscape) {
  state_.mode.Apply(ModeEvent::kEnterCommand);
  EXPECT_EQ(Send(Down("r", U'r')).disposition, KeyDisposition::kForward);
  EXPECT_EQ(Send(Down("Escape")).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(state_.mode.mode(), Mode::kNormal);

  state_.mode.Apply(ModeEvent::kEnterHint);
  EXPECT_EQ(Send(Down("Escape")).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(state_.mode.mode(), Mode::kNormal);
}

}  // namespace
}  // namespace vrowser
