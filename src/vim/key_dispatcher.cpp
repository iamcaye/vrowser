// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/key_dispatcher.hpp"

#include <algorithm>
#include <utility>

namespace vrowser {
namespace {

bool IsPlainEscape(const KeyEvent& event) {
  return event.key == "Escape" && event.modifiers == Modifiers{};
}

bool SamePress(const ConsumedPress& press, const KeyEvent& event) {
  return press.key == event.key && press.modifiers == event.modifiers;
}

bool IsModifierSubset(const Modifiers& subset, const Modifiers& full) {
  return (!subset.control || full.control) && (!subset.alt || full.alt) &&
         (!subset.meta || full.meta) && (!subset.shift || full.shift);
}

int ModifierCount(const Modifiers& modifiers) {
  return (modifiers.control ? 1 : 0) + (modifiers.alt ? 1 : 0) +
         (modifiers.meta ? 1 : 0) + (modifiers.shift ? 1 : 0);
}

void RememberPress(KeyInputState& state, const KeyEvent& event) {
  const bool already = std::any_of(
      state.consumed_keys.begin(), state.consumed_keys.end(),
      [&event](const ConsumedPress& press) { return SamePress(press, event); });
  if (!already) {
    state.consumed_keys.push_back(ConsumedPress{event.key, event.modifiers});
  }
}

void DropExactPress(KeyInputState& state, const KeyEvent& event) {
  const auto it = std::find_if(
      state.consumed_keys.begin(), state.consumed_keys.end(),
      [&event](const ConsumedPress& press) { return SamePress(press, event); });
  if (it != state.consumed_keys.end()) {
    state.consumed_keys.erase(it);
  }
}

// Tightest consumed press whose modifiers cover the key-up. A key-up with an
// extra modifier matches nothing.
std::vector<ConsumedPress>::iterator FindKeyUp(KeyInputState& state,
                                               const KeyEvent& event) {
  auto best = state.consumed_keys.end();
  int best_extra = 0;
  for (auto it = state.consumed_keys.begin(); it != state.consumed_keys.end();
       ++it) {
    if (it->key != event.key ||
        !IsModifierSubset(event.modifiers, it->modifiers)) {
      continue;
    }
    const int extra =
        ModifierCount(it->modifiers) - ModifierCount(event.modifiers);
    if (best == state.consumed_keys.end() || extra < best_extra) {
      best = it;
      best_extra = extra;
    }
  }
  return best;
}

KeyDispatch ConsumedResult() {
  KeyDispatch dispatch;
  dispatch.disposition = KeyDisposition::kConsume;
  return dispatch;
}

KeyDispatch FinishDown(KeyInputState& state,
                       const KeyEvent& event,
                       KeyDispatch dispatch) {
  if (dispatch.disposition == KeyDisposition::kConsume) {
    RememberPress(state, event);
    state.suppress_chars = true;
  } else {
    state.suppress_chars = false;
  }
  return dispatch;
}

KeyDispatch DispatchKeyUp(KeyInputState& state, const KeyEvent& event) {
  const auto it = FindKeyUp(state, event);
  if (it == state.consumed_keys.end()) {
    return KeyDispatch{};
  }
  state.consumed_keys.erase(it);
  if (state.consumed_keys.empty()) {
    state.suppress_chars = false;
  }
  return ConsumedResult();
}

KeyDispatch DispatchRepeat(KeyInputState& state, const KeyEvent& event) {
  const bool held = std::any_of(
      state.consumed_keys.begin(), state.consumed_keys.end(),
      [&event](const ConsumedPress& press) { return SamePress(press, event); });
  if (!held) {
    state.suppress_chars = false;
    return KeyDispatch{};
  }
  state.suppress_chars = true;
  return ConsumedResult();
}

KeyDispatch DispatchNormal(const KeyBindings& bindings,
                           KeyInputState& state,
                           const KeyEvent& event) {
  const bool pending = !state.sequence.keys.empty() || state.sequence.count > 0;
  if (IsPlainEscape(event)) {
    if (!pending) {
      return KeyDispatch{};
    }
    state.sequence = KeySequenceState{};
    return ConsumedResult();
  }

  KeyResolution resolution =
      bindings.Resolve(Mode::kNormal, state.sequence, KeyNotation(event));
  switch (resolution.status) {
    case KeyStatus::kMatched: {
      KeyDispatch dispatch = ConsumedResult();
      dispatch.action = std::move(resolution.action);
      dispatch.count = resolution.count;
      dispatch.has_count = resolution.has_count;
      return dispatch;
    }
    case KeyStatus::kPrefix:
      return ConsumedResult();
    case KeyStatus::kNoMatch:
      return pending ? ConsumedResult() : KeyDispatch{};
  }
  return KeyDispatch{};
}

}  // namespace

void NoteEditableFocus(KeyInputState& state, bool editable) {
  state.mode.Apply(editable ? ModeEvent::kEditableFocusGained
                            : ModeEvent::kEditableFocusLost);
  state.sequence = KeySequenceState{};
}

void NoteNavigation(KeyInputState& state) {
  state.consumed_keys.clear();
  state.suppress_chars = false;
}

KeyDispatch DispatchKey(const KeyBindings& bindings,
                        KeyInputState& state,
                        const KeyEvent& event) {
  if (event.character_only) {
    if (!state.suppress_chars) {
      return KeyDispatch{};
    }
    return ConsumedResult();
  }
  if (!event.key_down) {
    return DispatchKeyUp(state, event);
  }
  if (event.repeat) {
    return DispatchRepeat(state, event);
  }
  // A new press of a key that is still marked consumed means its key-up was
  // lost (a reload can drop it). Drop that press before handling this one.
  DropExactPress(state, event);
  if (state.mode.mode() == Mode::kNormal) {
    return FinishDown(state, event, DispatchNormal(bindings, state, event));
  }
  if (IsPlainEscape(event)) {
    state.mode.Apply(ModeEvent::kEscape);
    state.sequence = KeySequenceState{};
    return FinishDown(state, event, ConsumedResult());
  }
  return FinishDown(state, event, KeyDispatch{});
}

}  // namespace vrowser
