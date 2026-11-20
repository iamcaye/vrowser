// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace vrowser {

// Platform key event in the shape CEF reports it, without CEF types. The CEF
// adapter copies `CefKeyEvent` fields into this struct.
enum class RawKeyType {
  kRawKeyDown,  // Windows key-down, before translation to a character.
  kKeyDown,     // macOS and Linux key-down, usually with a character.
  kKeyUp,
  kChar,  // Windows character event that follows a raw key-down.
};

// Modifier bits. The values match `cef_event_flags_t` so the adapter can pass
// `CefKeyEvent::modifiers` through unchanged.
inline constexpr uint32_t kRawModifierShift = 1u << 1;
inline constexpr uint32_t kRawModifierControl = 1u << 2;
inline constexpr uint32_t kRawModifierAlt = 1u << 3;
inline constexpr uint32_t kRawModifierMeta = 1u << 7;  // Command on macOS.

struct RawKeyInput {
  RawKeyType type{RawKeyType::kKeyDown};
  // Windows virtual-key code. CEF reports it on every platform.
  int windows_key_code{0};
  // Character produced with the current modifiers, or 0.
  char32_t character{0};
  uint32_t modifiers{0};
};

struct Modifiers {
  bool control{false};
  bool alt{false};
  bool meta{false};
  bool shift{false};

  bool operator==(const Modifiers&) const = default;
};

// The one key type mode logic sees on every platform.
struct KeyEvent {
  // Layout-independent identity: "a".."z", "0".."9", or a name such as
  // "Escape", "Enter", "Tab", "Backspace", "Space", or "ArrowLeft".
  std::string key;
  // Printable character produced, or 0 for control characters and keys that
  // produce none.
  char32_t character{0};
  Modifiers modifiers;
  // True when the event should start an action.
  bool key_down{true};

  bool operator==(const KeyEvent&) const = default;
};

// Maps a platform event to a KeyEvent. Returns nullopt for character-only
// events, which duplicate a key-down, and for keys without an identity yet
// (lone modifiers, function keys).
std::optional<KeyEvent> NormalizeKey(const RawKeyInput& input);

}  // namespace vrowser
