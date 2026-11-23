// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace vrowser {

class CommandRegistry;
class KeyBindings;

// The user's config file, in the profile directory.
inline constexpr std::string_view kUserConfigFileName = "vrowserrc";

// Outcome of applying a config. Errors are values, not exceptions.
struct ConfigResult {
  bool ok = true;
  // "vrowserrc:3: Unknown command: foo". Empty when ok.
  std::string error;
};

// Applies config text to Normal-mode `bindings` and `commands`. One entry per
// line:
//
//   map <keys> <command> [args...]   bind keys, replacing an existing binding
//   unmap <keys>                     remove a binding
//   command <name> <step> [| <step>...]
//                                    a new command that runs each step, a
//                                    command line, in order, stopping at the
//                                    first error
//   " comment, # comment, or a blank line
//
// Commands named by `map` and by steps must be registered in `commands` or
// defined by an earlier `command` line. Either every line applies or none
// does: on an error `bindings` and `commands` are unchanged. New commands run
// their steps through `commands`, which must outlive them. `source` names the
// text in error messages.
ConfigResult ApplyUserConfig(std::string_view text,
                             std::string_view source,
                             CommandRegistry& commands,
                             KeyBindings& bindings);

// Reads `path` and applies it with ApplyUserConfig. A missing file is not an
// error and leaves `bindings` unchanged.
ConfigResult LoadUserConfig(const std::filesystem::path& path,
                            CommandRegistry& commands,
                            KeyBindings& bindings);

}  // namespace vrowser
