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

KeyDispatch Consume(KeyInputState& state, const KeyEvent& event) {
  if (std::find(state.consumed_keys.begin(), state.consumed_keys.end(),
                event.key) == state.consumed_keys.end()) {
    state.consumed_keys.push_back(event.key);
  }
  KeyDispatch dispatch;
  dispatch.disposition = KeyDisposition::kConsume;
  return dispatch;
}

KeyDispatch DispatchKeyUp(KeyInputState& state, const KeyEvent& event) {
  const auto it = std::find(state.consumed_keys.begin(),
                            state.consumed_keys.end(), event.key);
  if (it == state.consumed_keys.end()) {
    return KeyDispatch{};
  }
  state.consumed_keys.erase(it);
  KeyDispatch dispatch;
  dispatch.disposition = KeyDisposition::kConsume;
  return dispatch;
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
    return Consume(state, event);
  }

  KeyResolution resolution =
      bindings.Resolve(Mode::kNormal, state.sequence, KeyNotation(event));
  switch (resolution.status) {
    case KeyStatus::kMatched: {
      KeyDispatch dispatch = Consume(state, event);
      dispatch.action = std::move(resolution.action);
      dispatch.count = resolution.count;
      dispatch.has_count = resolution.has_count;
      return dispatch;
    }
    case KeyStatus::kPrefix:
      return Consume(state, event);
    case KeyStatus::kNoMatch:
      return pending ? Consume(state, event) : KeyDispatch{};
  }
  return KeyDispatch{};
}

}  // namespace

KeyDispatch DispatchKey(const KeyBindings& bindings,
                        KeyInputState& state,
                        const KeyEvent& event) {
  if (!event.key_down) {
    return DispatchKeyUp(state, event);
  }
  if (state.mode.mode() == Mode::kNormal) {
    return DispatchNormal(bindings, state, event);
  }
  if (IsPlainEscape(event)) {
    state.mode.Apply(ModeEvent::kEscape);
    state.sequence = KeySequenceState{};
    return Consume(state, event);
  }
  return KeyDispatch{};
}

}  // namespace vrowser
