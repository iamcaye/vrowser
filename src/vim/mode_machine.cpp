// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {

Mode Transition(Mode current, ModeEvent event) {
  switch (event) {
    case ModeEvent::kEscape:
      return Mode::kNormal;
    case ModeEvent::kEnterCommand:
      return Mode::kCommand;
    case ModeEvent::kEnterHint:
      return Mode::kHint;
    case ModeEvent::kEditableFocusGained:
      if (current == Mode::kCommand || current == Mode::kHint) {
        return current;
      }
      return Mode::kInsert;
    case ModeEvent::kEditableFocusLost:
      if (current == Mode::kInsert) {
        return Mode::kNormal;
      }
      return current;
  }
  return current;
}

void ModeMachine::Apply(ModeEvent event) {
  switch (event) {
    case ModeEvent::kEditableFocusGained:
      if (editable_focused_) {
        return;
      }
      editable_focused_ = true;
      mode_ = Transition(mode_, event);
      return;
    case ModeEvent::kEditableFocusLost:
      if (!editable_focused_) {
        return;
      }
      editable_focused_ = false;
      mode_ = Transition(mode_, event);
      return;
    case ModeEvent::kEscape:
    case ModeEvent::kEnterCommand:
    case ModeEvent::kEnterHint:
      mode_ = Transition(mode_, event);
      return;
  }
}

}  // namespace vrowser
