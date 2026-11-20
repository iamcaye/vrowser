// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_event.hpp"

#include <string_view>
#include <utility>

namespace vrowser {
namespace {

// Windows virtual-key codes for the named keys.
constexpr int kVkBack = 0x08;
constexpr int kVkTab = 0x09;
constexpr int kVkReturn = 0x0D;
constexpr int kVkEscape = 0x1B;
constexpr int kVkSpace = 0x20;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;

std::optional<std::string> KeyIdentity(int code) {
  if (code >= 'A' && code <= 'Z') {
    return std::string(1, static_cast<char>(code - 'A' + 'a'));
  }
  if (code >= '0' && code <= '9') {
    return std::string(1, static_cast<char>(code));
  }
  std::string_view name;
  switch (code) {
    case kVkBack:
      name = "Backspace";
      break;
    case kVkTab:
      name = "Tab";
      break;
    case kVkReturn:
      name = "Enter";
      break;
    case kVkEscape:
      name = "Escape";
      break;
    case kVkSpace:
      name = "Space";
      break;
    case kVkLeft:
      name = "ArrowLeft";
      break;
    case kVkUp:
      name = "ArrowUp";
      break;
    case kVkRight:
      name = "ArrowRight";
      break;
    case kVkDown:
      name = "ArrowDown";
      break;
    default:
      return std::nullopt;
  }
  return std::string(name);
}

bool IsPrintable(char32_t c) {
  return c >= 0x20 && c != 0x7F && !(c >= 0x80 && c < 0xA0);
}

}  // namespace

std::optional<KeyEvent> NormalizeKey(const RawKeyInput& input) {
  if (input.type == RawKeyType::kChar) {
    return std::nullopt;
  }
  std::optional<std::string> key = KeyIdentity(input.windows_key_code);
  if (!key) {
    return std::nullopt;
  }

  KeyEvent event;
  event.key = std::move(*key);
  event.character = IsPrintable(input.character) ? input.character : 0;
  event.modifiers.control = (input.modifiers & kRawModifierControl) != 0;
  event.modifiers.alt = (input.modifiers & kRawModifierAlt) != 0;
  event.modifiers.meta = (input.modifiers & kRawModifierMeta) != 0;
  event.modifiers.shift = (input.modifiers & kRawModifierShift) != 0;
  event.key_down = input.type != RawKeyType::kKeyUp;
  return event;
}

}  // namespace vrowser
