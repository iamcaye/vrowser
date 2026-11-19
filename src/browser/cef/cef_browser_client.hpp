// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <list>
#include <string>

#include "include/cef_client.h"

namespace vrowser {

class CefBrowserClient : public CefClient,
                         public CefDisplayHandler,
                         public CefLifeSpanHandler,
                         public CefLoadHandler {
 public:
  CefBrowserClient(bool alloy_style, bool smoke_test);
  ~CefBrowserClient() override;

  static CefBrowserClient* GetInstance();

  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  bool DoClose(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override;
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int http_status_code) override;
  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode error_code,
                   const CefString& error_text,
                   const CefString& failed_url) override;

  void ShowMainWindow();
  void CloseAllBrowsers(bool force_close);
  void HandleRelaunch(const std::string& url);
  bool IsClosing() const { return is_closing_; }
  int exit_code() const { return exit_code_; }

 private:
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);
  void OnSmokeTimeout();
  void FinishSmoke(int code);

  const bool alloy_style_;
  const bool smoke_test_;
  bool smoke_timer_armed_ = false;
  bool smoke_finished_ = false;
  int exit_code_ = 0;

  using BrowserList = std::list<CefRefPtr<CefBrowser>>;
  BrowserList browsers_;
  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(CefBrowserClient);
};

}  // namespace vrowser
