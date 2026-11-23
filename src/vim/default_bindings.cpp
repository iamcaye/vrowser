// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/vim/default_bindings.hpp"

namespace vrowser {

bool AddDefaultBindings(KeyBindings& bindings) {
  struct Entry {
    const char* sequence;
    const char* command;
  };
  static constexpr Entry kDefaults[] = {
      {":", "command-line"},
      {"r", "reload"},
      {"j", "scroll-down"},
      {"k", "scroll-up"},
      {"d", "scroll-half-page-down"},
      {"u", "scroll-half-page-up"},
      {"gg", "scroll-to-top"},
      {"G", "scroll-to-bottom"},
      {"H", "history-back"},
      {"L", "history-forward"},
      {"gt", "tab-next"},
      {"gT", "tab-previous"},
      {"t", "tab-new"},
      {"x", "tab-close"},
      {"f", "hint-follow"},
  };
  bool ok = true;
  for (const Entry& entry : kDefaults) {
    ok = bindings.Bind(Mode::kNormal, entry.sequence, entry.command) && ok;
  }
  // The command line opened with a command already typed.
  ok = bindings.Bind(Mode::kNormal, "o", "command-line", {"open"}) && ok;
  ok = bindings.Bind(Mode::kNormal, "O", "command-line", {"tabopen"}) && ok;
  return ok;
}

}  // namespace vrowser
