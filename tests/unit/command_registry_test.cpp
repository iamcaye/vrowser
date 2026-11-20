// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/commands/command_registry.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace vrowser {
namespace {

using Args = std::vector<std::string>;

TEST(ParseCommandLineTest, SplitsNameAndArguments) {
  const ParsedCommand parsed = ParseCommandLine("open https://example.com");
  EXPECT_EQ(parsed.name, "open");
  EXPECT_EQ(parsed.args, Args{"https://example.com"});
}

TEST(ParseCommandLineTest, CollapsesWhitespaceAroundTokens) {
  const ParsedCommand parsed = ParseCommandLine("  set \t foo   bar  ");
  EXPECT_EQ(parsed.name, "set");
  EXPECT_EQ(parsed.args, (Args{"foo", "bar"}));
}

TEST(ParseCommandLineTest, NameWithoutArguments) {
  const ParsedCommand parsed = ParseCommandLine("quit");
  EXPECT_EQ(parsed.name, "quit");
  EXPECT_TRUE(parsed.args.empty());
}

TEST(ParseCommandLineTest, EmptyAndBlankLinesHaveNoName) {
  EXPECT_TRUE(ParseCommandLine("").name.empty());
  EXPECT_TRUE(ParseCommandLine(" \t ").name.empty());
}

TEST(CommandRegistryTest, RegisteredCommandRunsWithArguments) {
  CommandRegistry registry;
  Args received;
  ASSERT_TRUE(
      registry.Register("open", "Open a URL", [&received](const Args& args) {
        received = args;
        return CommandResult::Ok();
      }));

  const CommandResult result = registry.Execute("open https://example.com x");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(received, (Args{"https://example.com", "x"}));
}

TEST(CommandRegistryTest, HandlerErrorIsReturned) {
  CommandRegistry registry;
  registry.Register("fail", "Always fails",
                    [](const Args&) { return CommandResult::Error("nope"); });

  const CommandResult result = registry.Execute("fail");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), "nope");
}

TEST(CommandRegistryTest, UnknownCommandReturnsError) {
  CommandRegistry registry;
  const CommandResult result = registry.Execute("missing arg");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), "Unknown command: missing");
}

TEST(CommandRegistryTest, EmptyLineReturnsError) {
  CommandRegistry registry;
  const CommandResult result = registry.Execute("   ");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error(), "Empty command");
}

TEST(CommandRegistryTest, RejectsDuplicateAndInvalidNames) {
  CommandRegistry registry;
  const auto noop = [](const Args&) { return CommandResult::Ok(); };
  EXPECT_TRUE(registry.Register("quit", "Quit", noop));
  EXPECT_FALSE(registry.Register("quit", "Again", noop));
  EXPECT_FALSE(registry.Register("", "Empty", noop));
  EXPECT_FALSE(registry.Register("two words", "Space", noop));
  EXPECT_FALSE(registry.Register("tab\tname", "Tab", noop));
  EXPECT_FALSE(registry.Register("null", "No handler", nullptr));
  EXPECT_EQ(registry.Description("quit"), "Quit");
}

TEST(CommandRegistryTest, DescriptionIsAvailableForCompletion) {
  CommandRegistry registry;
  const auto noop = [](const Args&) { return CommandResult::Ok(); };
  registry.Register("quit", "Close the window", noop);
  registry.Register("open", "Open a URL", noop);

  EXPECT_EQ(registry.Description("open"), "Open a URL");
  EXPECT_FALSE(registry.Description("missing").has_value());

  const std::vector<CommandInfo> commands = registry.List();
  ASSERT_EQ(commands.size(), 2u);
  EXPECT_EQ(commands[0].name, "open");
  EXPECT_EQ(commands[0].description, "Open a URL");
  EXPECT_EQ(commands[1].name, "quit");
  EXPECT_EQ(commands[1].description, "Close the window");
}

TEST(CommandRegistryTest, ContainsReportsRegistration) {
  CommandRegistry registry;
  EXPECT_FALSE(registry.Contains("open"));
  registry.Register("open", "Open a URL",
                    [](const Args&) { return CommandResult::Ok(); });
  EXPECT_TRUE(registry.Contains("open"));
}

}  // namespace
}  // namespace vrowser
