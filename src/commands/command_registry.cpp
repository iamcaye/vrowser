// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/commands/command_registry.hpp"

#include <algorithm>

namespace vrowser {
namespace {

bool IsSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
         c == '\v';
}

std::vector<std::string> SplitOnWhitespace(std::string_view line) {
  std::vector<std::string> tokens;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && IsSpace(line[i])) {
      ++i;
    }
    const size_t start = i;
    while (i < line.size() && !IsSpace(line[i])) {
      ++i;
    }
    if (i > start) {
      tokens.emplace_back(line.substr(start, i - start));
    }
  }
  return tokens;
}

}  // namespace

ParsedCommand ParseCommandLine(std::string_view line) {
  ParsedCommand parsed;
  std::vector<std::string> tokens = SplitOnWhitespace(line);
  if (tokens.empty()) {
    return parsed;
  }
  parsed.name = std::move(tokens.front());
  parsed.args.assign(std::make_move_iterator(tokens.begin() + 1),
                     std::make_move_iterator(tokens.end()));
  return parsed;
}

bool CommandRegistry::Register(std::string name,
                               std::string description,
                               CommandHandler handler) {
  if (name.empty() || !handler ||
      std::any_of(name.begin(), name.end(), IsSpace)) {
    return false;
  }
  return commands_
      .try_emplace(std::move(name),
                   Entry{std::move(description), std::move(handler)})
      .second;
}

bool CommandRegistry::Contains(std::string_view name) const {
  return commands_.find(name) != commands_.end();
}

std::optional<std::string> CommandRegistry::Description(
    std::string_view name) const {
  const auto it = commands_.find(name);
  if (it == commands_.end()) {
    return std::nullopt;
  }
  return it->second.description;
}

std::vector<CommandInfo> CommandRegistry::List() const {
  std::vector<CommandInfo> infos;
  infos.reserve(commands_.size());
  for (const auto& [name, entry] : commands_) {
    infos.push_back(CommandInfo{name, entry.description});
  }
  return infos;
}

CommandResult CommandRegistry::Execute(std::string_view line) const {
  const ParsedCommand parsed = ParseCommandLine(line);
  if (parsed.name.empty()) {
    return CommandResult::Error("Empty command");
  }
  return Run(parsed.name, parsed.args);
}

CommandResult CommandRegistry::Run(std::string_view name,
                                   const std::vector<std::string>& args,
                                   int count,
                                   bool has_count) const {
  const auto it = commands_.find(name);
  if (it == commands_.end()) {
    return CommandResult::Error("Unknown command: " + std::string(name));
  }
  // A typed count repeats the command. No count, or a count of 1, runs once.
  int times = 1;
  if (has_count && count > 1) {
    times = count;
  }
  CommandResult result = CommandResult::Ok();
  for (int i = 0; i < times; ++i) {
    result = it->second.handler(args);
    if (!result.ok()) {
      return result;
    }
  }
  return result;
}

}  // namespace vrowser
