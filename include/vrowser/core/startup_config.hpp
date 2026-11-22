// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace vrowser {

// Process startup options that do not depend on CEF. The desktop shell copies
// these into CefSettings and into the first browser window.
struct StartupConfig {
  std::string initial_url{"https://example.com"};
  std::filesystem::path user_data_dir;
  // Alloy style is the minimal window. The default follows cefsimple: the
  // Chrome bootstrap with the default (Chrome) runtime style.
  bool alloy_style{false};
  // Views is the cross-platform window. --use-native selects a platform window.
  bool use_views{true};
  // Load the initial URL, print one result line, and quit.
  bool smoke_test{false};
  // Like smoke_test, but checks the status bar shows the URL, opens a second
  // tab with `t`, switches tabs with
  // `gt` and `gT`, and closes both with `x`, so closing the last tab quits.
  // Implies smoke_test.
  bool tab_smoke_test{false};
  // Like smoke_test, but loads a built-in test page, sends keys, checks that
  // Normal-mode bindings are consumed, Insert mode follows a focused text
  // field, the scroll bindings move the page, and a held `j` keeps scrolling
  // and stops, and quits. Implies
  // smoke_test.
  bool key_smoke_test{false};
};

// Platform directory for profiles, cache, and logs.
std::filesystem::path DefaultUserDataDirectory();

// Parses Vrowser switches. Unknown arguments are ignored so CEF subprocess
// switches such as --type=renderer can pass through on Linux and Windows.
StartupConfig ParseStartupConfig(const std::vector<std::string>& args);
StartupConfig ParseStartupConfig(int argc, char* argv[]);

}  // namespace vrowser
