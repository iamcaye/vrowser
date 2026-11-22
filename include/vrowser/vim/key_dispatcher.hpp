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

// One consumed key-down, including the modifiers held when it went down.
struct ConsumedPress {
  std::string key;
  Modifiers modifiers;
  // The binding this key-down completed, if any.
  std::optional<BindingAction> action;
  // Set from the first OS repeat of a press that completed a binding until
  // its key-up.
  bool holding{false};

  bool operator==(const ConsumedPress&) const = default;
};

// Keyboard state for one tab.
struct KeyInputState {
  ModeMachine mode;
  KeySequenceState sequence;
  // Presses whose key-down was consumed. The matching key-up is consumed too.
  // A key-up matches when the key is the same and its modifiers are a subset
  // of the key-down's (a platform may release Shift before the key-up). A
  // key-up that adds a modifier the key-down did not have does not match, so
  // a stale plain "r" cannot swallow Shift+R.
  std::vector<ConsumedPress> consumed_keys;
  // Set while the most recent key-down was consumed, until its key-up. The
  // character events that follow that key-down are consumed too.
  bool suppress_chars{false};
};

enum class KeyDisposition {
  kForward,  // Let the page handle the key.
  kConsume,  // The shell handled it; the page must not see it.
};

// A key that completed a binding and is now held down.
enum class HoldEvent {
  kNone,
  kStart,     // First OS repeat of the press.
  kContinue,  // A later OS repeat.
  kStop,      // Its key-up.
};

struct KeyDispatch {
  KeyDisposition disposition{KeyDisposition::kForward};
  HoldEvent hold{HoldEvent::kNone};
  // The binding of the held press. Set when `hold` is not kNone. The caller
  // decides which commands keep acting while held; the dispatcher does not.
  std::optional<BindingAction> held_action;
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
//
// OS key repeat does not run a binding again. A repeat of a consumed press is
// consumed; when that press completed a binding, the repeat and the key-up
// are reported as a hold (see HoldEvent) so the caller can keep scrolling. A
// repeat of a forwarded key is forwarded, so typing still repeats. A non-repeat
// key-down of a press that is still marked consumed means the key-up was lost;
// that stale press is dropped before the new key-down is handled.
//
// A character-only event never starts a binding. It is consumed when the
// key-down it follows was consumed, so a Windows KEYEVENT_CHAR cannot leak
// after a consumed raw key-down.
KeyDispatch DispatchKey(const KeyBindings& bindings,
                        KeyInputState& state,
                        const KeyEvent& event);

// Focus reported by the renderer. Switches Normal/Insert and drops any
// pending count or key sequence. The page moved on; the old prefix did not.
void NoteEditableFocus(KeyInputState& state, bool editable);

// The shell moved from one tab to another while keys may still be down, as
// after `gt`. Moves the consumed presses of `from` to `to`, so their key-ups
// are consumed in the tab that receives them instead of reaching its page.
void TransferHeldKeys(KeyInputState& from, KeyInputState& to);

// Main-frame navigation. Drops consumed-press tracking so a key-up that the
// load discarded cannot swallow a later key-up of the same key.
void NoteNavigation(KeyInputState& state);

}  // namespace vrowser
