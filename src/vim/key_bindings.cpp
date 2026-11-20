// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_bindings.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace vrowser {
namespace {

void AppendUtf8(char32_t c, std::string& out) {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

// Byte length of the UTF-8 sequence that starts with `lead`. Invalid lead
// bytes count as one byte so parsing always advances.
size_t Utf8Length(unsigned char lead) {
  if (lead >= 0xF0 && lead < 0xF8) {
    return 4;
  }
  if (lead >= 0xE0) {
    return lead < 0xF0 ? 3 : 1;
  }
  if (lead >= 0xC0) {
    return 2;
  }
  return 1;
}

std::string_view NamedKeyNotation(std::string_view key) {
  if (key == "Escape") {
    return "Esc";
  }
  if (key == "Enter") {
    return "CR";
  }
  if (key == "Backspace") {
    return "BS";
  }
  if (key == "ArrowLeft") {
    return "Left";
  }
  if (key == "ArrowRight") {
    return "Right";
  }
  if (key == "ArrowUp") {
    return "Up";
  }
  if (key == "ArrowDown") {
    return "Down";
  }
  return key;  // "Tab", "Space", letters, and digits are already notation.
}

bool IsAsciiLetter(std::string_view key) {
  return key.size() == 1 && key[0] >= 'a' && key[0] <= 'z';
}

bool IsAsciiDigit(std::string_view key) {
  return key.size() == 1 && key[0] >= '0' && key[0] <= '9';
}

bool StartsCount(std::string_view key) {
  return IsAsciiDigit(key) && key[0] != '0';
}

}  // namespace

std::string KeyNotation(const KeyEvent& event) {
  const Modifiers& mods = event.modifiers;
  const bool command_modifier = mods.control || mods.alt || mods.meta;

  if (!command_modifier && event.key != "Space") {
    if (event.character != 0) {
      std::string out;
      AppendUtf8(event.character, out);
      return out;
    }
    if (IsAsciiLetter(event.key)) {
      return std::string(
          1, mods.shift ? static_cast<char>(event.key[0] - 32) : event.key[0]);
    }
    if (IsAsciiDigit(event.key) && !mods.shift) {
      return event.key;
    }
  }

  std::string out = "<";
  if (mods.control) {
    out += "C-";
  }
  if (mods.alt) {
    out += "A-";
  }
  if (mods.meta) {
    out += "D-";
  }
  if (mods.shift) {
    out += "S-";
  }
  out += NamedKeyNotation(event.key);
  out += '>';
  return out;
}

std::vector<std::string> ParseKeySequence(std::string_view sequence) {
  std::vector<std::string> keys;
  size_t i = 0;
  while (i < sequence.size()) {
    if (sequence[i] == '<') {
      const size_t close = sequence.find('>', i + 1);
      const size_t next_open = sequence.find('<', i + 1);
      if (close != std::string_view::npos && close > i + 1 &&
          (next_open == std::string_view::npos || next_open > close)) {
        keys.emplace_back(sequence.substr(i, close - i + 1));
        i = close + 1;
        continue;
      }
    }
    const size_t length =
        std::min(Utf8Length(static_cast<unsigned char>(sequence[i])),
                 sequence.size() - i);
    keys.emplace_back(sequence.substr(i, length));
    i += length;
  }
  return keys;
}

bool KeyBindings::HasPrefix(const Sequence& sequence, const Sequence& prefix) {
  return sequence.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), sequence.begin());
}

bool KeyBindings::Bind(Mode mode,
                       std::string_view sequence,
                       std::string command,
                       std::vector<std::string> args) {
  Sequence keys = ParseKeySequence(sequence);
  if (keys.empty() || command.empty()) {
    return false;
  }
  if (mode == Mode::kNormal && StartsCount(keys.front())) {
    return false;
  }
  Table& table = tables_[mode];

  // An existing binding that equals or extends `keys`.
  const auto it = table.lower_bound(keys);
  if (it != table.end() && HasPrefix(it->first, keys)) {
    return false;
  }
  // An existing binding that is a proper prefix of `keys`.
  for (size_t length = 1; length < keys.size(); ++length) {
    if (table.contains(Sequence(keys.begin(), keys.begin() + length))) {
      return false;
    }
  }

  table.emplace(std::move(keys),
                BindingAction{std::move(command), std::move(args)});
  return true;
}

KeyResolution KeyBindings::Resolve(Mode mode,
                                   KeySequenceState& state,
                                   std::string key) const {
  const bool counting = mode == Mode::kNormal && state.keys.empty();
  if (counting && IsAsciiDigit(key) && (state.count > 0 || StartsCount(key))) {
    state.count = std::min(kMaxCount, state.count * 10 + (key[0] - '0'));
    return KeyResolution{KeyStatus::kPrefix, {}, state.count, true};
  }

  state.keys.push_back(std::move(key));

  const auto table = tables_.find(mode);
  if (table != tables_.end()) {
    const auto it = table->second.lower_bound(state.keys);
    if (it != table->second.end() && it->first == state.keys) {
      KeyResolution result{KeyStatus::kMatched, it->second, 1, false};
      if (state.count > 0) {
        result.count = state.count;
        result.has_count = true;
      }
      state = KeySequenceState{};
      return result;
    }
    if (it != table->second.end() && HasPrefix(it->first, state.keys)) {
      return KeyResolution{
          KeyStatus::kPrefix, {}, std::max(1, state.count), state.count > 0};
    }
  }

  state = KeySequenceState{};
  return KeyResolution{KeyStatus::kNoMatch, {}, 1, false};
}

}  // namespace vrowser
