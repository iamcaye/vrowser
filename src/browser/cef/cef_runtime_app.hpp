// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "include/cef_app.h"
#include "vrowser/core/startup_config.hpp"

namespace vrowser {

class CefBrowserClient;

// Copies profile paths into CefSettings. Returns false when the user-data
// directory cannot be created. Safe to call before CefInitialize.
bool ConfigureCefSettings(CefSettings& settings, const StartupConfig& config);

class CefRuntimeApp : public CefApp, public CefBrowserProcessHandler {
 public:
  explicit CefRuntimeApp(StartupConfig config);

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;
  bool OnAlreadyRunningAppRelaunch(CefRefPtr<CefCommandLine> command_line,
                                   const CefString& current_directory) override;

  int exit_code() const;

 private:
  StartupConfig config_;
  CefRefPtr<CefBrowserClient> client_;

  IMPLEMENT_REFCOUNTING(CefRuntimeApp);
};

}  // namespace vrowser
