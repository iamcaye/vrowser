// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_bindings.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "vrowser/vim/key_event.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {
namespace {

using Keys = std::vector<std::string>;

KeyEvent Key(std::string key,
             char32_t character,
             Modifiers modifiers = Modifiers{}) {
  return KeyEvent{std::move(key), character, modifiers, true};
}

TEST(KeyNotationTest, PrintableCharacters) {
  EXPECT_EQ(KeyNotation(Key("j", U'j')), "j");
  EXPECT_EQ(KeyNotation(Key("g", U'G', {.shift = true})), "G");
  EXPECT_EQ(KeyNotation(Key("1", U'!', {.shift = true})), "!");
  // U+00E9 (e with acute) is written as a number: MSVC reads source files
  // in the system code page.
  EXPECT_EQ(KeyNotation(Key("e", char32_t{0xE9})), "\xC3\xA9");
}

TEST(KeyNotationTest, LettersWithoutCharacterUseKeyIdentity) {
  // Windows raw key-downs carry no character.
  EXPECT_EQ(KeyNotation(Key("j", 0)), "j");
  EXPECT_EQ(KeyNotation(Key("g", 0, {.shift = true})), "G");
  EXPECT_EQ(KeyNotation(Key("5", 0)), "5");
}

TEST(KeyNotationTest, NamedKeys) {
  EXPECT_EQ(KeyNotation(Key("Escape", 0)), "<Esc>");
  EXPECT_EQ(KeyNotation(Key("Enter", 0)), "<CR>");
  EXPECT_EQ(KeyNotation(Key("Tab", 0)), "<Tab>");
  EXPECT_EQ(KeyNotation(Key("Backspace", 0)), "<BS>");
  EXPECT_EQ(KeyNotation(Key("Space", U' ')), "<Space>");
  EXPECT_EQ(KeyNotation(Key("ArrowDown", 0)), "<Down>");
  EXPECT_EQ(KeyNotation(Key("Tab", 0, {.shift = true})), "<S-Tab>");
}

TEST(KeyNotationTest, ModifiedKeys) {
  EXPECT_EQ(KeyNotation(Key("d", 0, {.control = true})), "<C-d>");
  EXPECT_EQ(KeyNotation(Key("d", 0, {.control = true, .shift = true})),
            "<C-S-d>");
  EXPECT_EQ(KeyNotation(Key("t", U't', {.meta = true})), "<D-t>");
  EXPECT_EQ(KeyNotation(Key("x", U'x', {.alt = true})), "<A-x>");
}

TEST(ParseKeySequenceTest, SplitsCharactersAndBracketedKeys) {
  EXPECT_EQ(ParseKeySequence("gg"), (Keys{"g", "g"}));
  EXPECT_EQ(ParseKeySequence("<C-d>"), (Keys{"<C-d>"}));
  EXPECT_EQ(ParseKeySequence("g<Esc>T"), (Keys{"g", "<Esc>", "T"}));
  EXPECT_EQ(ParseKeySequence("<"), (Keys{"<"}));
  EXPECT_EQ(ParseKeySequence("<a"), (Keys{"<", "a"}));
  EXPECT_EQ(ParseKeySequence("\xC3\xA9x"), (Keys{"\xC3\xA9", "x"}));
  EXPECT_TRUE(ParseKeySequence("").empty());
}

class KeyBindingsTest : public ::testing::Test {
 protected:
  KeySequenceState state_;
  KeyBindings bindings_;
};

TEST_F(KeyBindingsTest, SingleKeyBindingResolves) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "j", "scroll-down"));
  const KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "j");
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll-down");
  EXPECT_TRUE(result.action.args.empty());
  EXPECT_TRUE(state_.keys.empty());
}

TEST_F(KeyBindingsTest, BindingCarriesArguments) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "<C-d>", "scroll", {"half-page"}));
  const KeyResolution result =
      bindings_.Resolve(Mode::kNormal, state_, "<C-d>");
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll");
  EXPECT_EQ(result.action.args, Keys{"half-page"});
}

TEST_F(KeyBindingsTest, TwoKeyBindingReportsPrefixThenResolves) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "gT", "tab-previous"));

  KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "g");
  EXPECT_EQ(result.status, KeyStatus::kPrefix);
  EXPECT_EQ(state_.keys, Keys{"g"});

  result = bindings_.Resolve(Mode::kNormal, state_, "T");
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "tab-previous");
  EXPECT_TRUE(state_.keys.empty());
}

TEST_F(KeyBindingsTest, UnknownSequenceClearsBuffer) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "gT", "tab-previous"));
  bindings_.Resolve(Mode::kNormal, state_, "g");
  ASSERT_EQ(state_.keys, Keys{"g"});

  const KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "x");
  EXPECT_EQ(result.status, KeyStatus::kNoMatch);
  EXPECT_TRUE(state_.keys.empty());

  EXPECT_EQ(bindings_.Resolve(Mode::kNormal, state_, "q").status,
            KeyStatus::kNoMatch);
  EXPECT_TRUE(state_.keys.empty());
}

TEST_F(KeyBindingsTest, BindingsArePerMode) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "f", "hint-open"));
  ASSERT_TRUE(bindings_.Bind(Mode::kHint, "f", "hint-select-f"));

  EXPECT_EQ(bindings_.Resolve(Mode::kInsert, state_, "f").status,
            KeyStatus::kNoMatch);
  EXPECT_EQ(bindings_.Resolve(Mode::kNormal, state_, "f").action.command,
            "hint-open");
  EXPECT_EQ(bindings_.Resolve(Mode::kHint, state_, "f").action.command,
            "hint-select-f");
}

TEST_F(KeyBindingsTest, RejectsEmptyDuplicateAndAmbiguousBindings) {
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "", "nothing"));
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "x", ""));
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "gg", "top"));
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "gg", "again"));
  // `g` would be both a binding and a prefix of `gg`. That needs a timeout.
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "g", "short"));
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "ggx", "long"));
  EXPECT_TRUE(bindings_.Bind(Mode::kNormal, "gT", "tab-previous"));
  EXPECT_TRUE(bindings_.Bind(Mode::kInsert, "g", "other-mode"));
}

TEST_F(KeyBindingsTest, ResolvesNormalizedKeyEvents) {
  ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "G", "bottom"));
  const KeyEvent shifted_g = Key("g", U'G', {.shift = true});
  EXPECT_EQ(bindings_.Resolve(Mode::kNormal, state_, KeyNotation(shifted_g))
                .action.command,
            "bottom");
}

class KeyCountTest : public KeyBindingsTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "j", "scroll-down"));
    ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "gg", "scroll-top"));
    ASSERT_TRUE(bindings_.Bind(Mode::kNormal, "0", "scroll-left-edge"));
    ASSERT_TRUE(bindings_.Bind(Mode::kHint, "1", "hint-label-1"));
  }

  KeyResolution Type(Mode mode, const Keys& keys) {
    KeyResolution result;
    for (const std::string& key : keys) {
      result = bindings_.Resolve(mode, state_, key);
    }
    return result;
  }
};

TEST_F(KeyCountTest, KeyWithoutCountHasCountOne) {
  const KeyResolution result = Type(Mode::kNormal, {"j"});
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll-down");
  EXPECT_EQ(result.count, 1);
  EXPECT_FALSE(result.has_count);
}

TEST_F(KeyCountTest, LeadingDigitsFormCount) {
  EXPECT_EQ(bindings_.Resolve(Mode::kNormal, state_, "1").status,
            KeyStatus::kPrefix);
  EXPECT_EQ(bindings_.Resolve(Mode::kNormal, state_, "2").status,
            KeyStatus::kPrefix);
  EXPECT_EQ(state_.count, 12);

  const KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "j");
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll-down");
  EXPECT_EQ(result.count, 12);
  EXPECT_TRUE(result.has_count);
  EXPECT_EQ(state_.count, 0);
  EXPECT_TRUE(state_.keys.empty());
}

TEST_F(KeyCountTest, ZeroExtendsAStartedCount) {
  const KeyResolution result = Type(Mode::kNormal, {"1", "0", "j"});
  EXPECT_EQ(result.action.command, "scroll-down");
  EXPECT_EQ(result.count, 10);
}

TEST_F(KeyCountTest, ZeroAsFirstKeyIsACommand) {
  const KeyResolution result = Type(Mode::kNormal, {"0"});
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll-left-edge");
  EXPECT_EQ(result.count, 1);
  EXPECT_FALSE(result.has_count);
}

TEST_F(KeyCountTest, MultiKeySequenceIsPrefixThenResolves) {
  KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "g");
  EXPECT_EQ(result.status, KeyStatus::kPrefix);
  EXPECT_EQ(state_.keys, Keys{"g"});

  result = bindings_.Resolve(Mode::kNormal, state_, "g");
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "scroll-top");
  EXPECT_EQ(result.count, 1);
}

TEST_F(KeyCountTest, CountAppliesToMultiKeySequence) {
  EXPECT_EQ(Type(Mode::kNormal, {"5", "g"}).status, KeyStatus::kPrefix);
  const KeyResolution result = bindings_.Resolve(Mode::kNormal, state_, "g");
  EXPECT_EQ(result.action.command, "scroll-top");
  EXPECT_EQ(result.count, 5);
  EXPECT_TRUE(result.has_count);
}

TEST_F(KeyCountTest, DigitInsideSequenceIsAnOrdinaryKey) {
  // `g` started a sequence, so `3` is looked up as a key and does not match.
  const KeyResolution result = Type(Mode::kNormal, {"g", "3"});
  EXPECT_EQ(result.status, KeyStatus::kNoMatch);
  EXPECT_TRUE(state_.keys.empty());
}

TEST_F(KeyCountTest, UnknownKeyClearsCount) {
  const KeyResolution result = Type(Mode::kNormal, {"4", "x"});
  EXPECT_EQ(result.status, KeyStatus::kNoMatch);
  EXPECT_EQ(state_.count, 0);
  EXPECT_EQ(Type(Mode::kNormal, {"j"}).count, 1);
}

TEST_F(KeyCountTest, CountSaturates) {
  const KeyResolution result =
      Type(Mode::kNormal, {"9", "9", "9", "9", "9", "9", "9", "j"});
  EXPECT_EQ(result.count, kMaxCount);
}

TEST_F(KeyCountTest, OtherModesDoNotTakeCounts) {
  const KeyResolution result = Type(Mode::kHint, {"1"});
  EXPECT_EQ(result.status, KeyStatus::kMatched);
  EXPECT_EQ(result.action.command, "hint-label-1");
  EXPECT_FALSE(result.has_count);
}

TEST_F(KeyCountTest, NormalBindingsCannotStartWithCountDigit) {
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "5", "five"));
  EXPECT_FALSE(bindings_.Bind(Mode::kNormal, "2x", "two-x"));
  EXPECT_TRUE(bindings_.Bind(Mode::kNormal, "g5", "g-five"));
}

}  // namespace
}  // namespace vrowser
