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
#include "vrowser/vim/key_dispatcher.hpp"
#include "vrowser/vim/key_event.hpp"
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
    ASSERT_TRUE(commands_.Register(
        "log", "", [this](const std::vector<std::string>& args) {
          std::string line;
          for (const std::string& arg : args) {
            line += (line.empty() ? "" : " ") + arg;
          }
          log_.push_back(line);
          return CommandResult::Ok();
        }));
    ASSERT_TRUE(commands_.Register(
        "fail", "", [](const std::vector<std::string>& /*args*/) {
          return CommandResult::Error("failed on purpose");
        }));
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
  [[nodiscard]] const std::vector<std::string>& log() const { return log_; }

 private:
  std::vector<std::string> log_;
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
      WriteConfig(std::string((1024 * 1024) + 1, '#')), commands(), bindings());

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(Normal("j"), (BindingAction{.command = "scroll-down"}));
}

TEST_F(UserConfigTest, CustomCommandRunsItsSequence) {
  ASSERT_TRUE(Apply("command greet log hello world | log again\n").ok);

  ASSERT_TRUE(commands().Contains("greet"));
  EXPECT_TRUE(commands().Execute("greet").ok());
  EXPECT_EQ(log(), (std::vector<std::string>{"hello world", "again"}));
  EXPECT_EQ(commands().Description("greet"), "vrowserrc: log | log");
}

TEST_F(UserConfigTest, KeymapInvokesACustomCommandFromNormalMode) {
  ASSERT_TRUE(Apply("command twice log one | log two\nmap Q twice\n").ok);
  KeyInputState state;
  ASSERT_EQ(state.mode.mode(), Mode::kNormal);

  const KeyDispatch dispatch =
      DispatchKey(bindings(), state,
                  KeyEvent{.key = "Q", .character = U'Q', .key_down = true});

  EXPECT_EQ(dispatch.disposition, KeyDisposition::kConsume);
  ASSERT_TRUE(dispatch.action.has_value());
  EXPECT_TRUE(commands()
                  .Run(dispatch.action->command, dispatch.action->args,
                       dispatch.count, dispatch.has_count)
                  .ok());
  EXPECT_EQ(log(), (std::vector<std::string>{"one", "two"}));
}

TEST_F(UserConfigTest, CustomCommandCanUseAnEarlierCustomCommand) {
  ASSERT_TRUE(Apply("command a log a\ncommand b a | log b\n").ok);

  EXPECT_TRUE(commands().Execute("b").ok());
  EXPECT_EQ(log(), (std::vector<std::string>{"a", "b"}));
}

TEST_F(UserConfigTest, CustomCommandStopsAtTheFirstFailingStep) {
  ASSERT_TRUE(Apply("command risky log before | fail | log after\n").ok);

  const CommandResult result = commands().Execute("risky");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), "failed on purpose");
  EXPECT_EQ(log(), (std::vector<std::string>{"before"}));
}

struct InvalidCommandCase {
  const char* text;
  const char* error;
};

class InvalidCommandTest
    : public UserConfigTest,
      public ::testing::WithParamInterface<InvalidCommandCase> {};

TEST_P(InvalidCommandTest, IsRejectedAndRegistersNothing) {
  const ConfigResult result = Apply(GetParam().text);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.error, GetParam().error);
  EXPECT_FALSE(commands().Contains("custom"));
  EXPECT_FALSE(Normal("Q").has_value());
}

INSTANTIATE_TEST_SUITE_P(
    Entries,
    InvalidCommandTest,
    ::testing::Values(
        InvalidCommandCase{
            .text = "command custom log ok | no-such-command",
            .error = "vrowserrc:1: Unknown command: no-such-command",
        },
        // A command cannot call itself: only earlier commands are known.
        InvalidCommandCase{
            .text = "command custom custom",
            .error = "vrowserrc:1: Unknown command: custom",
        },
        InvalidCommandCase{
            .text = "command reload log x",
            .error = "vrowserrc:1: Command already exists: reload",
        },
        InvalidCommandCase{
            .text = "command custom log x\ncommand custom log y",
            .error = "vrowserrc:2: Command already exists: custom",
        },
        InvalidCommandCase{
            .text = "command custom log x | | log y",
            .error = "vrowserrc:1: command custom has an empty step",
        },
        InvalidCommandCase{
            .text = "command custom",
            .error = "vrowserrc:1: command needs a name and at least one step",
        },
        // A valid command and keymap, then a bad line: neither applies.
        InvalidCommandCase{
            .text = "command custom log x\nmap Q custom\nmap j nope",
            .error = "vrowserrc:3: Unknown command: nope",
        }));

}  // namespace
}  // namespace vrowser
