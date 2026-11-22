// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vrowser {

// Outcome of parsing or running a command. Errors are values, not exceptions,
// so the module stays usable from targets built with -fno-exceptions.
class CommandResult {
 public:
  static CommandResult Ok() { return CommandResult(); }
  static CommandResult Error(std::string message) {
    return CommandResult(std::move(message));
  }

  bool ok() const { return !error_.has_value(); }
  // Empty when ok().
  std::string_view error() const {
    return error_ ? std::string_view(*error_) : std::string_view();
  }

 private:
  CommandResult() = default;
  explicit CommandResult(std::string message) : error_(std::move(message)) {}

  std::optional<std::string> error_;
};

// One input line split on whitespace. `name` is empty for a blank line.
struct ParsedCommand {
  std::string name;
  std::vector<std::string> args;
};

// Splits a command line into a name and arguments. Quoting is not supported.
ParsedCommand ParseCommandLine(std::string_view line);

using CommandHandler =
    std::function<CommandResult(const std::vector<std::string>& args)>;
// A handler that takes the Normal-mode count itself instead of being repeated,
// such as `5j`, which scrolls five steps in one call.
using CountedCommandHandler =
    std::function<CommandResult(const std::vector<std::string>& args,
                                int count)>;

// Name and one-line description, for completion and help.
struct CommandInfo {
  std::string name;
  std::string description;
};

// Maps command names to handlers. Key bindings, the command line, and a later
// CLI all run commands through one registry.
class CommandRegistry {
 public:
  // Returns false when the name is empty, contains whitespace, is already
  // registered, or the handler is empty.
  bool Register(std::string name,
                std::string description,
                CommandHandler handler);
  // Same rules as Register. Run calls the handler once with the count.
  bool RegisterCounted(std::string name,
                       std::string description,
                       CountedCommandHandler handler);

  bool Contains(std::string_view name) const;
  std::optional<std::string> Description(std::string_view name) const;
  // Registered commands sorted by name.
  std::vector<CommandInfo> List() const;

  // Parses `line` and runs the named command. Unknown or empty input returns
  // an error result.
  CommandResult Execute(std::string_view line) const;

  // Runs a command by name, as a key binding does. Unknown names return an
  // error result. When `has_count` is set and `count` is greater than 1, a
  // plain handler runs that many times (a Normal-mode count such as `3r`),
  // and a counted handler runs once with `count`. Otherwise a counted handler
  // gets 1.
  CommandResult Run(std::string_view name,
                    const std::vector<std::string>& args,
                    int count = 1,
                    bool has_count = false) const;

 private:
  struct Entry {
    std::string description;
    // Exactly one of the two is set.
    CommandHandler handler;
    CountedCommandHandler counted;
  };

  std::map<std::string, Entry, std::less<>> commands_;
};

}  // namespace vrowser
