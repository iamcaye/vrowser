// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <list>
#include <map>
#include <string>

#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "vrowser/core/navigation.hpp"
#include "vrowser/core/startup_config.hpp"
#include "vrowser/commands/command_registry.hpp"
#include "vrowser/core/tab_list.hpp"
#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/key_dispatcher.hpp"

namespace vrowser {

class CefBrowserClient : public CefClient,
                         public CefDisplayHandler,
                         public CefKeyboardHandler,
                         public CefLifeSpanHandler,
                         public CefLoadHandler {
 public:
  explicit CefBrowserClient(const StartupConfig& config);

  // data: URL of the page --vrowser-key-smoke-test loads.
  static std::string KeySmokePageUrl();
  ~CefBrowserClient() override;

  static CefBrowserClient* GetInstance();

  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // Normal-mode keys that match a binding are consumed here, before the page
  // sees them. Insert-mode keys go to the page, except Escape.
  bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                     const CefKeyEvent& event,
                     CefEventHandle os_event,
                     bool* is_keyboard_shortcut) override;

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
  // Loads `url` in the open window. Safe to call from any thread; the request
  // is recorded and loaded on the UI thread.
  void Navigate(const std::string& url);
  // Opens `url` in a new tab of the main window and shows it. Requires Views
  // and Alloy style.
  // Safe to call from any thread.
  void OpenTab(const std::string& url);
  // Closes one tab. Closing the last tab closes the window and quits. Safe to
  // call from any thread.
  void CloseTab(TabId id);
  // Window close handler: starts closing every browser in `window` and
  // returns true once all of them are ready to be closed. UI thread.
  bool CanCloseWindow(CefRefPtr<CefWindow> window);
  bool IsClosing() const { return is_closing_; }
  int exit_code() const { return exit_code_; }

 private:
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);
  // macOS only: delivers a synthesized NSEvent key-down and key-up to the
  // browser's window, which takes the same path as a typed key.
  void PlatformSendTestKey(CefRefPtr<CefBrowser> browser,
                           int mac_key_code,
                           char16_t character);
  void LoadInMainFrame(const std::string& url);
  CefRefPtr<CefBrowser> FindBrowser(TabId id) const;
  CefRefPtr<CefBrowser> ActiveBrowser() const;
  // Shows the active tab's view and hides the other tabs' views.
  void ShowActiveTab();
  void TabSmokeLoadEnd(CefRefPtr<CefBrowser> browser);
  void TabSmokeClosed(TabId id);
  void RegisterCommands();
  void KeySmokeStep(CefRefPtr<CefBrowser> browser, const std::string& event);
  void SendTestKey(CefRefPtr<CefBrowser> browser,
                   int windows_key_code,
                   int mac_key_code,
                   int linux_key_code,
                   char16_t character);
  void RunBinding(const KeyDispatch& dispatch);
  void OnSmokeTimeout();
  void FinishSmoke(int code);

  const bool alloy_style_;
  const bool smoke_test_;
  const bool tab_smoke_test_;
  const bool key_smoke_test_;
  // Key smoke: index of the step waiting for its event. See KeySmokeStep.
  int key_smoke_step_ = 0;
  // Tab smoke: 0 waits for the first load, 1 for the second tab's load, 2 for
  // the first tab to close.
  int tab_smoke_stage_ = 0;
  TabId tab_smoke_first_ = 0;
  bool smoke_timer_armed_ = false;
  bool smoke_finished_ = false;
  int exit_code_ = 0;

  using BrowserList = std::list<CefRefPtr<CefBrowser>>;
  BrowserList browsers_;
  bool is_closing_ = false;
  // UI thread only. Its listener calls LoadInMainFrame.
  NavigationController navigation_;
  // Tabs of the main window, UI thread only. Popups have their own windows
  // and are not tabs.
  TabList tabs_;
  // Keyboard state per browser, UI thread only.
  std::map<TabId, KeyInputState> key_inputs_;
  KeyBindings bindings_;
  CommandRegistry commands_;

  IMPLEMENT_REFCOUNTING(CefBrowserClient);
};

}  // namespace vrowser
