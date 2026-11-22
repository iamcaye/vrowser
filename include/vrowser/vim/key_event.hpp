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
  // Key went down. Windows and windowed Linux CEF report this
  // (KEYEVENT_RAWKEYDOWN), often followed by a separate character event.
  kRawKeyDown,
  // macOS key-down, usually already carrying a character. Windowed Linux
  // CEF does not report real keys this way; it uses kRawKeyDown.
  kKeyDown,
  kKeyUp,
  // Character event that can follow a key-down (Windows, and Linux when the
  // key-down was not consumed). It is not another key-down. The dispatcher
  // consumes it when the key-down it follows was consumed.
  kChar,
};

// Modifier bits. The values match `cef_event_flags_t` so the adapter can pass
// `CefKeyEvent::modifiers` through unchanged.
inline constexpr uint32_t kRawModifierShift = 1u << 1;
inline constexpr uint32_t kRawModifierControl = 1u << 2;
inline constexpr uint32_t kRawModifierAlt = 1u << 3;
inline constexpr uint32_t kRawModifierMeta = 1u << 7;  // Command on macOS.
// OS key repeat. Not a modifier: NormalizeKey copies it to KeyEvent::repeat.
inline constexpr uint32_t kRawModifierRepeat = 1u << 13;

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
  // True for a key-down. False for a key-up or a character-only event.
  bool key_down{true};
  // OS auto-repeat of a key that is still held. The first key-down is not a
  // repeat.
  bool repeat{false};
  // True for a character event that follows a key-down. It must not start a
  // binding; the dispatcher only decides whether to consume it.
  bool character_only{false};

  bool operator==(const KeyEvent&) const = default;
};

// Maps a platform event to a KeyEvent. Character events are returned with
// `character_only` set so they are not treated as a second key-down. Returns
// nullopt for keys without an identity yet (lone modifiers, function keys).
std::optional<KeyEvent> NormalizeKey(const RawKeyInput& input);

}  // namespace vrowser
