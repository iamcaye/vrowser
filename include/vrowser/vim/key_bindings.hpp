// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "vrowser/vim/key_event.hpp"
#include "vrowser/vim/mode_machine.hpp"

namespace vrowser {

// One key in Vim notation: a printable character ("j", "G", "!") or a
// bracketed key ("<Esc>", "<CR>", "<Tab>", "<BS>", "<Space>", "<Down>",
// "<C-d>", "<S-Tab>", "<D-t>"). Modifier order is C, A, D (Meta), S.
std::string KeyNotation(const KeyEvent& event);

// Splits a binding such as "gT" or "g<C-d>" into notation tokens. A "<" with
// no closing ">" is a literal "<".
std::vector<std::string> ParseKeySequence(std::string_view sequence);

// What a binding runs: a command name for the command registry plus
// arguments.
struct BindingAction {
  std::string command;
  std::vector<std::string> args;

  bool operator==(const BindingAction&) const = default;
};

enum class KeyStatus {
  kMatched,  // `action` is set and the buffer was cleared.
  kPrefix,   // The buffer is a count, or the start of at least one binding.
  kNoMatch,  // Nothing starts with the buffer; it and the count were cleared.
};

// Largest count. Longer digit runs saturate here instead of overflowing.
inline constexpr int kMaxCount = 99999;

struct KeyResolution {
  KeyStatus status{KeyStatus::kNoMatch};
  BindingAction action;
  // Repeat count for a match: the typed count, or 1 when none was typed.
  int count{1};
  // True when the user typed a count, so `G` and `1G` can differ.
  bool has_count{false};
};

// Keys typed so far toward a multi-key binding, plus a pending count. Owned by
// the caller so the bindings table stays shared and const.
struct KeySequenceState {
  std::vector<std::string> keys;
  int count{0};  // 0 while no count digit has been typed.
};

// Per-mode table from key sequences to command names.
class KeyBindings {
 public:
  // `sequence` uses the notation from ParseKeySequence. Returns false when the
  // sequence or command is empty, the sequence is already bound, or it would
  // be both a binding and a prefix of another binding in the same mode.
  // Resolving that ambiguity needs a timeout, which is not implemented.
  // Normal-mode sequences cannot start with 1-9, which always begin a count.
  bool Bind(Mode mode,
            std::string_view sequence,
            std::string command,
            std::vector<std::string> args = {});

  // Appends `key` to `state` and looks the sequence up in `mode`. In Normal
  // mode, leading digits form a count: 1-9 start one, and 0 extends a count
  // that has started. A 0 with no count, or any digit after the first key of
  // a sequence, is an ordinary key. Other modes do not take counts.
  KeyResolution Resolve(Mode mode,
                        KeySequenceState& state,
                        std::string key) const;

 private:
  using Sequence = std::vector<std::string>;
  using Table = std::map<Sequence, BindingAction>;

  static bool HasPrefix(const Sequence& sequence, const Sequence& prefix);

  std::map<Mode, Table> tables_;
};

}  // namespace vrowser
