// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_event.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace vrowser {
namespace {

// Fixtures follow the shape CEF reports on each platform: macOS and Linux send
// a key-down that already carries the character, Windows sends a raw key-down
// (often without a character) followed by a separate character event.
RawKeyInput MacKeyDown(int key_code, char32_t character, uint32_t flags = 0) {
  return RawKeyInput{RawKeyType::kKeyDown, key_code, character, flags};
}

RawKeyInput WinRawKeyDown(int key_code, uint32_t flags = 0) {
  return RawKeyInput{RawKeyType::kRawKeyDown, key_code, 0, flags};
}

TEST(NormalizeKeyTest, LettersHaveLowercaseIdentity) {
  const std::optional<KeyEvent> event = NormalizeKey(MacKeyDown('K', U'k'));
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->key, "k");
  EXPECT_EQ(event->character, U'k');
  EXPECT_EQ(event->modifiers, Modifiers{});
  EXPECT_TRUE(event->key_down);
}

TEST(NormalizeKeyTest, EveryLetterMaps) {
  for (char c = 'A'; c <= 'Z'; ++c) {
    const auto event = NormalizeKey(WinRawKeyDown(c));
    ASSERT_TRUE(event.has_value()) << c;
    EXPECT_EQ(event->key, std::string(1, static_cast<char>(c - 'A' + 'a')));
  }
}

TEST(NormalizeKeyTest, NamedKeysHaveStableIdentities) {
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x1B, 0x1B))->key, "Escape");
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x0D, U'\r'))->key, "Enter");
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x09, U'\t'))->key, "Tab");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x1B))->key, "Escape");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x0D))->key, "Enter");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x09))->key, "Tab");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x08))->key, "Backspace");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x20))->key, "Space");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x25))->key, "ArrowLeft");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x26))->key, "ArrowUp");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x27))->key, "ArrowRight");
  EXPECT_EQ(NormalizeKey(WinRawKeyDown(0x28))->key, "ArrowDown");
}

TEST(NormalizeKeyTest, ControlCharactersAreNotProduced) {
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x1B, 0x1B))->character, 0u);
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x0D, U'\r'))->character, 0u);
  EXPECT_EQ(NormalizeKey(MacKeyDown(0x09, U'\t'))->character, 0u);
}

TEST(NormalizeKeyTest, ShiftChangesCharacterAndSetsModifier) {
  const auto plain = NormalizeKey(MacKeyDown('A', U'a'));
  const auto shifted = NormalizeKey(MacKeyDown('A', U'A', kRawModifierShift));
  ASSERT_TRUE(plain && shifted);
  EXPECT_EQ(plain->key, shifted->key);
  EXPECT_EQ(plain->character, U'a');
  EXPECT_EQ(shifted->character, U'A');
  EXPECT_FALSE(plain->modifiers.shift);
  EXPECT_TRUE(shifted->modifiers.shift);
}

TEST(NormalizeKeyTest, ShiftedDigitKeepsDigitIdentity) {
  const auto event = NormalizeKey(MacKeyDown('1', U'!', kRawModifierShift));
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->key, "1");
  EXPECT_EQ(event->character, U'!');
  EXPECT_TRUE(event->modifiers.shift);
}

TEST(NormalizeKeyTest, ControlLetterKeepsLetterIdentity) {
  // macOS and Linux report Ctrl+A with the control character U+0001.
  const auto event = NormalizeKey(MacKeyDown('A', 0x01, kRawModifierControl));
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->key, "a");
  EXPECT_EQ(event->character, 0u);
  EXPECT_TRUE(event->modifiers.control);
  EXPECT_FALSE(event->modifiers.shift);
}

TEST(NormalizeKeyTest, MapsAllModifiers) {
  const uint32_t all = kRawModifierShift | kRawModifierControl |
                       kRawModifierAlt | kRawModifierMeta;
  const auto event = NormalizeKey(WinRawKeyDown(0x09, all));
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(
      event->modifiers,
      (Modifiers{.control = true, .alt = true, .meta = true, .shift = true}));
}

TEST(NormalizeKeyTest, IgnoresUnrelatedFlags) {
  // Caps Lock and mouse-button flags are not key modifiers.
  const auto event = NormalizeKey(WinRawKeyDown('J', 1u << 0 | 1u << 4));
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->modifiers, Modifiers{});
}

TEST(NormalizeKeyTest, KeyUpIsNotAnAction) {
  const auto event = NormalizeKey(RawKeyInput{RawKeyType::kKeyUp, 'J', 0, 0});
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->key, "j");
  EXPECT_FALSE(event->key_down);
}

TEST(NormalizeKeyTest, CharacterOnlyEventsAreDropped) {
  // Windows follows the raw key-down with a character event. Acting on both
  // would run a binding twice.
  EXPECT_FALSE(
      NormalizeKey(RawKeyInput{RawKeyType::kChar, 'J', U'j', 0}).has_value());
}

TEST(NormalizeKeyTest, UnknownKeyCodesAreDropped) {
  EXPECT_FALSE(NormalizeKey(WinRawKeyDown(0)).has_value());
  EXPECT_FALSE(NormalizeKey(WinRawKeyDown(0x10)).has_value());  // Shift alone.
  EXPECT_FALSE(NormalizeKey(WinRawKeyDown(0xFF)).has_value());
}

}  // namespace
}  // namespace vrowser
