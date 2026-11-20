// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/key_event.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {

// Keyboard state for one tab.
struct KeyInputState {
  ModeMachine mode;
  KeySequenceState sequence;
  // Keys whose key-down was consumed, so their key-up is consumed too.
  std::vector<std::string> consumed_keys;
};

enum class KeyDisposition {
  kForward,  // Let the page handle the key.
  kConsume,  // The shell handled it; the page must not see it.
};

struct KeyDispatch {
  KeyDisposition disposition{KeyDisposition::kForward};
  // Set when a Normal-mode binding completed.
  std::optional<BindingAction> action;
  int count{1};
  bool has_count{false};
};

// Decides what one key does in the tab's current mode.
//
// Normal: keys that complete or extend a binding are consumed. A key that
// matches nothing is forwarded, unless it abandons a pending sequence or
// count, which is consumed and discarded. Escape clears a pending sequence.
// Insert: keys go to the page, except Escape, which returns to Normal and is
// consumed. Command and Hint have no UI yet: Escape returns to Normal, other
// keys are forwarded.
KeyDispatch DispatchKey(const KeyBindings& bindings,
                        KeyInputState& state,
                        const KeyEvent& event);

}  // namespace vrowser
