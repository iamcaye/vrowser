// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/config/user_config.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "vrowser/commands/command_registry.hpp"
#include "vrowser/vim/default_bindings.hpp"
#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {
namespace {

namespace fs = std::filesystem;

class UserConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (const char* name :
         {"reload", "scroll-down", "scroll-up", "scroll-to-top", "open"}) {
      ASSERT_TRUE(commands_.Register(
          name, "", [](const auto&) { return CommandResult::Ok(); }));
    }
    ASSERT_TRUE(AddDefaultBindings(bindings_));

    static std::atomic<int> counter{0};
    dir_ = fs::temp_directory_path() /
           ("vrowser-config-" +
            std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
            "-" + std::to_string(counter++));
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }

  void TearDown() override { fs::remove_all(dir_); }

  fs::path WriteConfig(const std::string& text) {
    const fs::path path = dir_ / std::string(kUserConfigFileName);
    std::ofstream(path, std::ios::binary) << text;
    return path;
  }

  ConfigResult Apply(const std::string& text) {
    return ApplyUserConfig(text, "vrowserrc", commands_, bindings_);
  }

  [[nodiscard]] std::optional<BindingAction> Normal(const char* keys) const {
    return bindings_.Find(Mode::kNormal, keys);
  }

  [[nodiscard]] const fs::path& dir() const { return dir_; }
  CommandRegistry& commands() { return commands_; }
  KeyBindings& bindings() { return bindings_; }

 private:
  CommandRegistry commands_;
  KeyBindings bindings_;
  fs::path dir_;
};

TEST_F(UserConfigTest, MissingFileKeepsDefaults) {
  const ConfigResult result =
      LoadUserConfig(dir() / "missing" / "vrowserrc", commands(), bindings());

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-down"}));
  EXPECT_EQ(Normal("r"), (BindingAction{.command = "reload"}));
}

TEST_F(UserConfigTest, KeymapEntryOverridesOneDefaultBinding) {
  const ConfigResult result =
      LoadUserConfig(WriteConfig("map j scroll-up\n"), commands(), bindings());

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-up"}));
  EXPECT_EQ(Normal("k"), (BindingAction{.command = "scroll-up"}));
  EXPECT_EQ(Normal("r"), (BindingAction{.command = "reload"}));
}

TEST_F(UserConfigTest, MapPassesArgumentsAndAddsNewSequences) {
  const ConfigResult result = Apply("map ,n open https://neovim.io\n");

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(Normal(",n"),
            (BindingAction{.command = "open", .args = {"https://neovim.io"}}));
}

TEST_F(UserConfigTest, UnmapRemovesABinding) {
  const ConfigResult result = Apply("unmap r\n");

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_FALSE(Normal("r").has_value());
}

TEST_F(UserConfigTest, CommentsBlankLinesAndCrLfAreIgnored) {
  const ConfigResult result = Apply(
      "\" Vim-style comment\r\n# hash comment\r\n\r\n  map j scroll-up  "
      "\r\n");

  EXPECT_TRUE(result.ok) << result.error;
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-up"}));
}

TEST_F(UserConfigTest, InvalidFileReportsTheLineAndKeepsPreviousBindings) {
  const ConfigResult result = LoadUserConfig(
      WriteConfig("map j scroll-up\nunmap r\nmap x no-such-command\n"),
      commands(), bindings());

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, "vrowserrc:3: Unknown command: no-such-command");
  // Lines 1 and 2 were valid, but nothing applies when any line fails.
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-down"}));
  EXPECT_EQ(Normal("r"), (BindingAction{.command = "reload"}));
}

struct InvalidCase {
  const char* text;
  const char* error;
};

class InvalidConfigTest : public UserConfigTest,
                          public ::testing::WithParamInterface<InvalidCase> {};

TEST_P(InvalidConfigTest, ReportsTheProblemAndChangesNothing) {
  const ConfigResult result = Apply(GetParam().text);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, GetParam().error);
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-down"}));
  EXPECT_EQ(Normal("gg"), (BindingAction{.command = "scroll-to-top"}));
}

INSTANTIATE_TEST_SUITE_P(
    Entries,
    InvalidConfigTest,
    ::testing::Values(
        InvalidCase{.text = "set smooth",
                    .error = "vrowserrc:1: Unknown entry: set"},
        InvalidCase{.text = "map j",
                    .error = "vrowserrc:1: map needs keys and a command"},
        InvalidCase{
            .text = "unmap",
            .error = "vrowserrc:1: unmap needs exactly one key sequence"},
        InvalidCase{.text = "unmap Q",
                    .error = "vrowserrc:1: No mapping for Q"},
        // "g" is a prefix of "gg", so it cannot also be a binding.
        InvalidCase{
            .text = "map j scroll-up\nmap g reload",
            .error = "vrowserrc:2: Cannot map g: it starts with a count "
                     "digit, or overlaps another binding"},
        InvalidCase{
            .text = "map 5 reload",
            .error = "vrowserrc:1: Cannot map 5: it starts with a count "
                     "digit, or overlaps another binding"}));

TEST_F(UserConfigTest, OverridesAMultiKeyBinding) {
  ASSERT_TRUE(Apply("map gg reload").ok);
  EXPECT_EQ(Normal("gg"), (BindingAction{.command = "reload"}));
}

TEST_F(UserConfigTest, OversizedFileIsRejected) {
  const ConfigResult result = LoadUserConfig(
      WriteConfig(std::string(1024 * 1024 + 1, '#')), commands(), bindings());

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-down"}));
}

}  // namespace
}  // namespace vrowser
