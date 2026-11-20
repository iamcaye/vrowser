// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/mode_machine.hpp"

#include <gtest/gtest.h>

namespace vrowser {
namespace {

TEST(ModeMachineTest, StartsInNormal) {
  const ModeMachine machine;
  EXPECT_EQ(machine.mode(), Mode::kNormal);
  EXPECT_FALSE(machine.editable_focused());
}

TEST(ModeMachineTest, EditableFocusForcesInsert) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  EXPECT_TRUE(machine.editable_focused());
  EXPECT_EQ(machine.mode(), Mode::kInsert);
}

TEST(ModeMachineTest, ClearingEditableFocusReturnsToNormalFromInsert) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  machine.Apply(ModeEvent::kEditableFocusLost);
  EXPECT_FALSE(machine.editable_focused());
  EXPECT_EQ(machine.mode(), Mode::kNormal);
}

TEST(ModeMachineTest, EscapeFromInsertReturnsToNormal) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  machine.Apply(ModeEvent::kEscape);
  EXPECT_EQ(machine.mode(), Mode::kNormal);
  EXPECT_TRUE(machine.editable_focused());
}

TEST(ModeMachineTest, EscapeFromCommandReturnsToNormal) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEnterCommand);
  EXPECT_EQ(machine.mode(), Mode::kCommand);
  machine.Apply(ModeEvent::kEscape);
  EXPECT_EQ(machine.mode(), Mode::kNormal);
}

TEST(ModeMachineTest, EscapeFromHintReturnsToNormal) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEnterHint);
  EXPECT_EQ(machine.mode(), Mode::kHint);
  machine.Apply(ModeEvent::kEscape);
  EXPECT_EQ(machine.mode(), Mode::kNormal);
}

TEST(ModeMachineTest, EnterCommandAndHintAreSeparateRequests) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEnterCommand);
  EXPECT_EQ(machine.mode(), Mode::kCommand);
  machine.Apply(ModeEvent::kEnterHint);
  EXPECT_EQ(machine.mode(), Mode::kHint);
}

TEST(ModeMachineTest, ClearingEditableFocusKeepsCommandMode) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  machine.Apply(ModeEvent::kEnterCommand);
  EXPECT_EQ(machine.mode(), Mode::kCommand);
  machine.Apply(ModeEvent::kEditableFocusLost);
  EXPECT_FALSE(machine.editable_focused());
  EXPECT_EQ(machine.mode(), Mode::kCommand);
}

TEST(ModeMachineTest, ClearingEditableFocusKeepsHintMode) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  machine.Apply(ModeEvent::kEnterHint);
  machine.Apply(ModeEvent::kEditableFocusLost);
  EXPECT_EQ(machine.mode(), Mode::kHint);
}

TEST(ModeMachineTest, EditableFocusDoesNotStealCommandMode) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEnterCommand);
  machine.Apply(ModeEvent::kEditableFocusGained);
  EXPECT_TRUE(machine.editable_focused());
  EXPECT_EQ(machine.mode(), Mode::kCommand);
}

TEST(ModeMachineTest, EscapeWhileEditableStaysNormalUntilFocusCycles) {
  ModeMachine machine;
  machine.Apply(ModeEvent::kEditableFocusGained);
  machine.Apply(ModeEvent::kEscape);
  EXPECT_EQ(machine.mode(), Mode::kNormal);

  // Still focused: a duplicate gained event must not flip back to Insert.
  machine.Apply(ModeEvent::kEditableFocusGained);
  EXPECT_EQ(machine.mode(), Mode::kNormal);

  machine.Apply(ModeEvent::kEditableFocusLost);
  machine.Apply(ModeEvent::kEditableFocusGained);
  EXPECT_EQ(machine.mode(), Mode::kInsert);
}

TEST(TransitionTest, CoversAllModes) {
  EXPECT_EQ(Transition(Mode::kNormal, ModeEvent::kEnterCommand),
            Mode::kCommand);
  EXPECT_EQ(Transition(Mode::kNormal, ModeEvent::kEnterHint), Mode::kHint);
  EXPECT_EQ(Transition(Mode::kNormal, ModeEvent::kEditableFocusGained),
            Mode::kInsert);
  EXPECT_EQ(Transition(Mode::kInsert, ModeEvent::kEscape), Mode::kNormal);
  EXPECT_EQ(Transition(Mode::kCommand, ModeEvent::kEscape), Mode::kNormal);
  EXPECT_EQ(Transition(Mode::kHint, ModeEvent::kEscape), Mode::kNormal);
  EXPECT_EQ(Transition(Mode::kInsert, ModeEvent::kEditableFocusLost),
            Mode::kNormal);
  EXPECT_EQ(Transition(Mode::kCommand, ModeEvent::kEditableFocusLost),
            Mode::kCommand);
}

}  // namespace
}  // namespace vrowser
