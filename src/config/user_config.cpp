// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/config/user_config.hpp"

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

// Applies one parsed `map` entry. Returns an empty string or the problem.
std::string ApplyMap(const std::vector<std::string>& args,
                     const CommandRegistry& commands,
                     KeyBindings& bindings) {
  if (args.size() < 2) {
    return "map needs keys and a command";
  }
  const std::string& keys = args[0];
  const std::string& command = args[1];
  if (!commands.Contains(command)) {
    return "Unknown command: " + command;
  }
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

std::string ApplyEntry(const ParsedCommand& entry,
                       const CommandRegistry& commands,
                       KeyBindings& bindings) {
  if (entry.name == "map") {
    return ApplyMap(entry.args, commands, bindings);
  }
  if (entry.name == "unmap") {
    if (entry.args.size() != 1) {
      return "unmap needs exactly one key sequence";
    }
    if (!bindings.Unbind(Mode::kNormal, entry.args[0])) {
      return "No mapping for " + entry.args[0];
    }
    return {};
  }
  return "Unknown entry: " + entry.name;
}

}  // namespace

ConfigResult ApplyUserConfig(std::string_view text,
                             std::string_view source,
                             const CommandRegistry& commands,
                             KeyBindings& bindings) {
  KeyBindings updated = bindings;
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
      std::string problem = ApplyEntry(entry, commands, updated);
      if (!problem.empty()) {
        return {
            .ok = false,
            .error = LineError(source, line_number, std::move(problem)),
        };
      }
    }
    start = end + 1;
  }
  bindings = std::move(updated);
  return {};
}

ConfigResult LoadUserConfig(const std::filesystem::path& path,
                            const CommandRegistry& commands,
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
