// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "browser/cef/cef_runtime_app.hpp"
#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"

#if defined(CEF_USE_BOOTSTRAP) && !defined(CEF_BOOTSTRAP_EXPORT)
#define CEF_BOOTSTRAP_EXPORT __declspec(dllexport)
#endif

namespace {

int RunMain(HINSTANCE instance, void* sandbox_info) {
  CefMainArgs main_args(instance);
  const int subprocess = CefExecuteProcess(main_args, nullptr, sandbox_info);
  if (subprocess >= 0) {
    return subprocess;
  }

  int argc = 0;
  LPWSTR* wide_argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
  std::vector<std::string> args;
  if (wide_argv != nullptr) {
    for (int i = 1; i < argc; ++i) {
      const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1,
                                              nullptr, 0, nullptr, nullptr);
      if (bytes > 1) {
        std::string utf8(static_cast<size_t>(bytes - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, utf8.data(), bytes,
                              nullptr, nullptr);
        args.push_back(std::move(utf8));
      }
    }
    ::LocalFree(wide_argv);
  }

  const vrowser::StartupConfig config = vrowser::ParseStartupConfig(args);
  CefSettings settings;
  if (sandbox_info == nullptr) {
    settings.no_sandbox = 1;
  }
  if (!vrowser::ConfigureCefSettings(settings, config)) {
    return 1;
  }

  CefRefPtr<vrowser::CefRuntimeApp> app(new vrowser::CefRuntimeApp(config));
  if (!CefInitialize(main_args, settings, app.get(), sandbox_info)) {
    return CefGetExitCode();
  }

  CefRunMessageLoop();
  const int exit_code = app->exit_code();
  CefShutdown();
  return exit_code;
}

}  // namespace

#if defined(CEF_USE_BOOTSTRAP)

CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE instance,
                                    LPWSTR command_line,
                                    int show_command,
                                    void* sandbox_info,
                                    cef_version_info_t* /*version_info*/) {
  (void)command_line;
  (void)show_command;
  return ::RunMain(instance, sandbox_info);
}

#else

int APIENTRY wWinMain(HINSTANCE instance,
                      HINSTANCE previous_instance,
                      LPWSTR command_line,
                      int show_command) {
  (void)previous_instance;
  (void)command_line;
  (void)show_command;

#if defined(ARCH_CPU_32_BITS)
  const int stacked = CefRunWinMainWithPreferredStackSize(
      wWinMain, instance, command_line, show_command);
  if (stacked >= 0) {
    return stacked;
  }
#endif

  void* sandbox_info = nullptr;
#if defined(CEF_USE_SANDBOX)
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif
  return ::RunMain(instance, sandbox_info);
}

#endif
