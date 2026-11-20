// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace vrowser {

// Keyboard modes for the shell. The machine does not parse key sequences.
enum class Mode {
  kNormal,
  kInsert,
  kCommand,
  kHint,
};

// Explicit inputs that change mode. Focus events come from the host; Escape
// and mode requests come from higher-level key handling.
enum class ModeEvent {
  kEscape,
  kEnterCommand,
  kEnterHint,
  kEditableFocusGained,
  kEditableFocusLost,
};

// Pure transition used by ModeMachine and by tests that do not need state.
Mode Transition(Mode current, ModeEvent event);

// Tracks the current mode and whether an editable element is focused.
// Editable focus forces Insert on the rising edge. Clearing editable focus
// returns to Normal only when the mode is still Insert.
class ModeMachine {
 public:
  Mode mode() const { return mode_; }
  bool editable_focused() const { return editable_focused_; }

  void Apply(ModeEvent event);

 private:
  Mode mode_{Mode::kNormal};
  bool editable_focused_{false};
};

}  // namespace vrowser
