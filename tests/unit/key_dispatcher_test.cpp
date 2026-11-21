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

KeyEvent Up(std::string key, Modifiers mods = {}) {
  return KeyEvent{std::move(key), 0, mods, false};
}

KeyEvent RepeatDown(std::string key, char32_t character = 0) {
  KeyEvent event{std::move(key), character, Modifiers{}, true};
  event.repeat = true;
  return event;
}

KeyEvent CharOnly(char32_t character) {
  KeyEvent event;
  event.character = character;
  event.key_down = false;
  event.character_only = true;
  return event;
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

TEST_F(KeyDispatcherTest, NavigationDropsALostKeyUp) {
  EXPECT_EQ(Send(Down("r", U'r')).disposition, KeyDisposition::kConsume);
  NoteNavigation(state_);
  EXPECT_TRUE(state_.consumed_keys.empty());
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, NewPressReplacesALostKeyUp) {
  EXPECT_EQ(Send(Down("r", U'r')).disposition, KeyDisposition::kConsume);
  state_.mode.Apply(ModeEvent::kEditableFocusGained);

  const KeyDispatch typed = Send(Down("r", U'r'));
  EXPECT_EQ(typed.disposition, KeyDisposition::kForward);
  EXPECT_FALSE(typed.action.has_value());
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, StalePlainKeyDoesNotConsumeShiftedKeyUp) {
  EXPECT_EQ(Send(Down("r", U'r')).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(Send(Up("r", {.shift = true})).disposition,
            KeyDisposition::kForward);
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kConsume);
}

TEST_F(KeyDispatcherTest, KeyUpStillMatchesWhenShiftWasReleasedFirst) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "R", "reload"));
  const KeyDispatch down = Send(Down("r", U'R', {.shift = true}));
  EXPECT_EQ(down.disposition, KeyDisposition::kConsume);
  ASSERT_TRUE(down.action.has_value());
  EXPECT_EQ(down.action->command, "reload");
  // X11 delivers the Shift key-up before the letter key-up.
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kConsume);
}

TEST_F(KeyDispatcherTest, RepeatDoesNotRunTheBindingAgain) {
  ASSERT_TRUE(Send(Down("r", U'r')).action.has_value());
  const KeyDispatch again = Send(RepeatDown("r", U'r'));
  EXPECT_EQ(again.disposition, KeyDisposition::kConsume);
  EXPECT_FALSE(again.action.has_value());
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kConsume);
}

TEST_F(KeyDispatcherTest, RepeatOfForwardedKeyIsForwarded) {
  EXPECT_EQ(Send(Down("x", U'x')).disposition, KeyDisposition::kForward);
  EXPECT_EQ(Send(RepeatDown("x", U'x')).disposition, KeyDisposition::kForward);
  EXPECT_EQ(Send(Up("x")).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, CharAfterConsumedKeyDownIsConsumed) {
  EXPECT_EQ(Send(Down("r", U'r')).disposition, KeyDisposition::kConsume);
  const KeyDispatch character = Send(CharOnly(U'r'));
  EXPECT_EQ(character.disposition, KeyDisposition::kConsume);
  EXPECT_FALSE(character.action.has_value());
  EXPECT_EQ(Send(Up("r")).disposition, KeyDisposition::kConsume);
  // The key-up ends the character window.
  EXPECT_EQ(Send(CharOnly(U'r')).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, CharAfterForwardedKeyDownIsForwarded) {
  EXPECT_EQ(Send(Down("x", U'x')).disposition, KeyDisposition::kForward);
  EXPECT_EQ(Send(CharOnly(U'x')).disposition, KeyDisposition::kForward);
}

TEST_F(KeyDispatcherTest, EditableFocusClearsPendingSequence) {
  EXPECT_EQ(Send(Down("3", U'3')).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(Send(Down("g", U'g')).disposition, KeyDisposition::kConsume);
  EXPECT_EQ(state_.sequence.count, 3);
  EXPECT_FALSE(state_.sequence.keys.empty());

  NoteEditableFocus(state_, true);
  EXPECT_EQ(state_.mode.mode(), Mode::kInsert);
  EXPECT_EQ(state_.sequence.count, 0);
  EXPECT_TRUE(state_.sequence.keys.empty());

  NoteEditableFocus(state_, false);
  EXPECT_EQ(state_.mode.mode(), Mode::kNormal);
  const KeyDispatch dispatch = Send(Down("r", U'r'));
  EXPECT_EQ(dispatch.count, 1);
  EXPECT_FALSE(dispatch.has_count);
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
