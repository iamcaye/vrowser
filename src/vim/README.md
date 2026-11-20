# vim

Mode state, key normalization, and, later, key sequence parsing. This module
must not include CEF headers.

- `ModeMachine` (`mode_machine.cpp`): Normal, Insert, Command, and Hint.
- `NormalizeKey` (`key_event.cpp`): maps a CEF-shaped `RawKeyInput` to a
  `KeyEvent` with a layout-independent key name, the produced character, and
  modifiers. The CEF adapter will copy `CefKeyEvent` fields into
  `RawKeyInput`; the modifier bits match `cef_event_flags_t`.
- `KeyBindings` (`key_bindings.cpp`): per-mode bindings from Vim-notation
  sequences (`j`, `gT`, `<C-d>`) to a command name and arguments. `Resolve`
  reports a match, a prefix, or no match; no match clears the buffer.
  `KeyNotation` turns a `KeyEvent` into one notation token.
