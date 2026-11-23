// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/config/user_config.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "vrowser/commands/command_registry.hpp"
#include "vrowser/vim/key_bindings.hpp"

namespace vrowser {
namespace {

// Largest config file read. A keymap file is a few kilobytes.
constexpr std::uintmax_t kMaxConfigBytes = std::uintmax_t{1024} * 1024;

std::string LineError(std::string_view source, int line, std::string message) {
  return std::string(source) + ":" + std::to_string(line) + ": " +
         std::move(message);
}

bool IsComment(const ParsedCommand& entry) {
  return entry.name.empty() || entry.name.front() == '"' ||
         entry.name.front() == '#';
}

// A `command` entry, validated but not registered yet.
struct CustomCommand {
  std::string name;
  std::vector<ParsedCommand> steps;
};

// Everything a config changes, applied only after every line is valid.
struct PendingConfig {
  const CommandRegistry* registry;
  KeyBindings bindings;
  std::vector<CustomCommand> commands;

  [[nodiscard]] bool Knows(std::string_view name) const {
    return registry->Contains(name) ||
           std::ranges::any_of(commands, [name](const CustomCommand& command) {
             return command.name == name;
           });
  }
};

// Applies one parsed `map` entry. Returns an empty string or the problem.
std::string ApplyMap(const std::vector<std::string>& args,
                     PendingConfig& config) {
  if (args.size() < 2) {
    return "map needs keys and a command";
  }
  const std::string& keys = args[0];
  const std::string& command = args[1];
  if (!config.Knows(command)) {
    return "Unknown command: " + command;
  }
  KeyBindings& bindings = config.bindings;
  const std::optional<BindingAction> previous =
      bindings.Find(Mode::kNormal, keys);
  bindings.Unbind(Mode::kNormal, keys);
  std::vector<std::string> command_args(args.begin() + 2, args.end());
  if (!bindings.Bind(Mode::kNormal, keys, command, std::move(command_args))) {
    if (previous) {
      bindings.Bind(Mode::kNormal, keys, previous->command, previous->args);
    }
    return "Cannot map " + keys +
           ": it starts with a count digit, or overlaps another binding";
  }
  return {};
}

// `command <name> <step> [| <step>...]`, each step a command line.
std::string ApplyCommand(const std::vector<std::string>& args,
                         PendingConfig& config) {
  if (args.size() < 2) {
    return "command needs a name and at least one step";
  }
  CustomCommand command{.name = args.front(), .steps = {{}}};
  if (config.Knows(command.name)) {
    return "Command already exists: " + command.name;
  }
  for (auto it = args.begin() + 1; it != args.end(); ++it) {
    if (*it == "|") {
      command.steps.emplace_back();
    } else if (command.steps.back().name.empty()) {
      command.steps.back().name = *it;
    } else {
      command.steps.back().args.push_back(*it);
    }
  }
  for (const ParsedCommand& step : command.steps) {
    if (step.name.empty()) {
      return "command " + command.name + " has an empty step";
    }
    // Only earlier commands are known, so a command cannot call itself.
    if (!config.Knows(step.name)) {
      return "Unknown command: " + step.name;
    }
  }
  config.commands.push_back(std::move(command));
  return {};
}

std::string ApplyEntry(const ParsedCommand& entry, PendingConfig& config) {
  if (entry.name == "map") {
    return ApplyMap(entry.args, config);
  }
  if (entry.name == "unmap") {
    if (entry.args.size() != 1) {
      return "unmap needs exactly one key sequence";
    }
    if (!config.bindings.Unbind(Mode::kNormal, entry.args[0])) {
      return "No mapping for " + entry.args[0];
    }
    return {};
  }
  if (entry.name == "command") {
    return ApplyCommand(entry.args, config);
  }
  return "Unknown entry: " + entry.name;
}

// Registers a validated custom command. Its steps run through `registry`,
// which must outlive it, and stop at the first error.
void RegisterCustomCommand(CustomCommand command, CommandRegistry& registry) {
  std::string description = "vrowserrc:";
  for (const ParsedCommand& step : command.steps) {
    description += (&step == &command.steps.front() ? " " : " | ") + step.name;
  }
  const bool registered = registry.Register(
      command.name, description,
      [&registry, steps = std::move(command.steps)](
          const std::vector<std::string>& /*args*/) {
        for (const ParsedCommand& step : steps) {
          CommandResult result = registry.Run(step.name, step.args);
          if (!result.ok()) {
            return result;
          }
        }
        return CommandResult::Ok();
      });
  // The name was checked against the registry while validating.
  (void)registered;
}

}  // namespace

ConfigResult ApplyUserConfig(std::string_view text,
                             std::string_view source,
                             CommandRegistry& commands,
                             KeyBindings& bindings) {
  PendingConfig pending{.registry = &commands, .bindings = bindings};
  int line_number = 0;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    ++line_number;
    const ParsedCommand entry =
        ParseCommandLine(text.substr(start, end - start));
    if (!IsComment(entry)) {
      std::string problem = ApplyEntry(entry, pending);
      if (!problem.empty()) {
        return {
            .ok = false,
            .error = LineError(source, line_number, std::move(problem)),
        };
      }
    }
    start = end + 1;
  }
  for (CustomCommand& command : pending.commands) {
    RegisterCustomCommand(std::move(command), commands);
  }
  bindings = std::move(pending.bindings);
  return {};
}

ConfigResult LoadUserConfig(const std::filesystem::path& path,
                            CommandRegistry& commands,
                            KeyBindings& bindings) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return {};
  }
  const std::string source = path.filename().string();
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > kMaxConfigBytes) {
    return {
        .ok = false,
        .error = source + ": cannot read, or larger than 1 MiB",
    };
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {.ok = false, .error = source + ": cannot open"};
  }
  const std::string text((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  return ApplyUserConfig(text, source, commands, bindings);
}

}  // namespace vrowser
