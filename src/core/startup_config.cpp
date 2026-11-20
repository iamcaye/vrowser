// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "vrowser/core/startup_config.hpp"

#include <cstdlib>
#include <optional>
#include <string_view>
#include <system_error>

namespace vrowser {
namespace {

bool HasSwitch(const std::vector<std::string>& args, std::string_view name) {
  const std::string flag = std::string("--") + std::string(name);
  const std::string prefix = flag + "=";
  for (const std::string& arg : args) {
    if (arg == flag || arg.starts_with(prefix)) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> SwitchValue(const std::vector<std::string>& args,
                                       std::string_view name) {
  const std::string flag = std::string("--") + std::string(name);
  const std::string prefix = flag + "=";
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == flag) {
      if (i + 1 < args.size() && !args[i + 1].starts_with("-")) {
        return args[i + 1];
      }
      return std::nullopt;
    }
    if (arg.starts_with(prefix)) {
      return arg.substr(prefix.size());
    }
  }
  return std::nullopt;
}

std::filesystem::path HomePath(const char* env_name) {
  const char* value = std::getenv(env_name);
  if (value == nullptr || value[0] == '\0') {
    return {};
  }
  return std::filesystem::path(value);
}

}  // namespace

std::filesystem::path DefaultUserDataDirectory() {
  namespace fs = std::filesystem;
#if defined(_WIN32)
  const fs::path local = HomePath("LOCALAPPDATA");
  if (!local.empty()) {
    return local / "Vrowser";
  }
  const fs::path profile = HomePath("USERPROFILE");
  if (!profile.empty()) {
    return profile / "AppData" / "Local" / "Vrowser";
  }
#elif defined(__APPLE__)
  const fs::path home = HomePath("HOME");
  if (!home.empty()) {
    return home / "Library" / "Application Support" / "Vrowser";
  }
#else
  const fs::path data_home = HomePath("XDG_DATA_HOME");
  if (!data_home.empty()) {
    return data_home / "vrowser";
  }
  const fs::path home = HomePath("HOME");
  if (!home.empty()) {
    return home / ".local" / "share" / "vrowser";
  }
#endif
  std::error_code error;
  const fs::path current = fs::current_path(error);
  if (error) {
    return fs::path("vrowser-user-data");
  }
  return current / "vrowser-user-data";
}

StartupConfig ParseStartupConfig(const std::vector<std::string>& args) {
  StartupConfig config;
  config.user_data_dir = DefaultUserDataDirectory();
  config.alloy_style = HasSwitch(args, "use-alloy-style");
  config.use_views = !HasSwitch(args, "use-native");
  config.tab_smoke_test = HasSwitch(args, "vrowser-tab-smoke-test");
  config.smoke_test =
      config.tab_smoke_test || HasSwitch(args, "vrowser-smoke-test");

  if (const std::optional<std::string> url = SwitchValue(args, "url")) {
    if (!url->empty()) {
      config.initial_url = *url;
    }
  }
  if (const std::optional<std::string> dir =
          SwitchValue(args, "user-data-dir")) {
    if (!dir->empty()) {
      config.user_data_dir = std::filesystem::path(*dir);
    }
  }
  return config;
}

StartupConfig ParseStartupConfig(int argc, char* argv[]) {
  std::vector<std::string> args;
  if (argc > 1 && argv != nullptr) {
    args.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
      if (argv[i] != nullptr) {
        args.emplace_back(argv[i]);
      }
    }
  }
  return ParseStartupConfig(args);
}

}  // namespace vrowser
