// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "include/cef_app.h"
#include "include/cef_request_context.h"
#include "vrowser/core/startup_config.hpp"

namespace vrowser {

class CefBrowserClient;

// Copies profile paths into CefSettings: the user-data directory is the root
// cache path, and the selected profile's directory is the cache path. Returns
// false when the profile name is invalid or its directory cannot be created.
// Safe to call before CefInitialize.
bool ConfigureCefSettings(CefSettings& settings, const StartupConfig& config);

class CefRuntimeApp : public CefApp, public CefBrowserProcessHandler {
 public:
  explicit CefRuntimeApp(StartupConfig config);
  ~CefRuntimeApp() override;

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;
  bool OnAlreadyRunningAppRelaunch(CefRefPtr<CefCommandLine> command_line,
                                   const CefString& current_directory) override;

  int exit_code() const;

  // Opens the first window in the profile's request context, once that
  // context has initialized.
  void OpenFirstWindow(CefRefPtr<CefRequestContext> request_context);

 private:
  StartupConfig config_;
  CefRefPtr<CefBrowserClient> client_;
  // Keeps the profile's context alive while it initializes.
  CefRefPtr<CefRequestContext> request_context_;
  bool first_window_opened_ = false;

  IMPLEMENT_REFCOUNTING(CefRuntimeApp);
};

}  // namespace vrowser
