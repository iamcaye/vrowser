// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "vrowser/core/navigation.hpp"
#include "vrowser/core/scroll.hpp"
#include "vrowser/core/startup_config.hpp"
#include "vrowser/commands/command_registry.hpp"
#include "vrowser/core/tab_list.hpp"
#include "vrowser/vim/key_bindings.hpp"
#include "vrowser/vim/key_dispatcher.hpp"

namespace vrowser {

// A key a smoke test types, with its code on each platform.
struct TestKey {
  int windows_key_code;
  int mac_key_code;
  int linux_key_code;
  char16_t character;
  bool shift = false;
};

// What a smoke test does with a key: a full press, or one part of a hold.
enum class TestKeyPhase {
  kPress,   // Key-down then key-up.
  kDown,    // Key-down only.
  kRepeat,  // An OS auto-repeat key-down.
  kUp,      // Key-up only.
};

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

  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool is_loading,
                            bool can_go_back,
                            bool can_go_forward) override;

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
  // A key-up seen by the platform before Chromium handles it. macOS drops
  // the key-up of a key-down that OnPreKeyEvent consumed, so this is how a
  // held scroll learns its key was released there. UI thread.
  void OnNativeKeyUp(int native_key_code);
  // The main window's status bar (Alloy style). UI thread.
  void AttachStatusBar(CefRefPtr<CefTextfield> status_bar);
  int exit_code() const { return exit_code_; }

 private:
  void PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                           const CefString& title);
  void PlatformShowWindow(CefRefPtr<CefBrowser> browser);
  // macOS: installs (or removes) an app-local key-up monitor that calls
  // OnNativeKeyUp. Other platforms deliver those key-ups to OnPreKeyEvent and
  // do nothing here.
  void PlatformWatchKeyUps(bool watch);
  // macOS only: delivers a synthesized NSEvent key-down and key-up to the
  // browser's window, which takes the same path as a typed key.
  void PlatformSendTestKey(CefRefPtr<CefBrowser> browser,
                           const TestKey& key,
                           TestKeyPhase phase);
  void LoadInMainFrame(const std::string& url);
  CefRefPtr<CefBrowser> FindBrowser(TabId id) const;
  CefRefPtr<CefBrowser> ActiveBrowser() const;
  // The browser a command acts on: the one whose key ran it, or the active
  // tab when no key is being handled.
  CefRefPtr<CefBrowser> CommandBrowser() const;
  // Shows the active tab's view and hides the other tabs' views.
  void ShowActiveTab();
  // Takes a closed tab's view out of its panel, then focuses the active tab.
  void RemoveTabView(CefRefPtr<CefPanel> tabs, CefRefPtr<CefBrowserView> view);
  void TabSmokeLoadEnd(CefRefPtr<CefBrowser> browser);
  void TabSmokeActivated(TabId id);
  // Tab smoke: logs the status bar and checks it shows the browser's URL.
  bool TabSmokeStatusBarShows(CefRefPtr<CefBrowser> browser);
  void TabSmokeClosed(TabId id);
  // Types `keys` into `browser`, in order, from UI tasks.
  void SendTestKeys(CefRefPtr<CefBrowser> browser,
                    std::initializer_list<TestKey> keys,
                    int64_t delay_ms = 0);
  void RegisterCommands();
  // Scrolls the focused frame of CommandBrowser(), or its main frame when no
  // frame has focus.
  void ScrollFocusedFrame(ScrollMotion motion, int count);
  // Moves `steps` tabs right (positive) or left (negative) from the active
  // tab, wrapping, and shows and focuses the new tab.
  void SwitchTab(int steps);
  // Sets the window title from the active tab's current page.
  void ShowActiveTitle();
  // Redraws the status bar for the active tab: committed main-frame URL,
  // mode, pending keys, loading. Does nothing without a status bar.
  void UpdateStatusBar();
  // Current status bar text, empty without one. For the smoke tests.
  std::string StatusBarText() const;
  void KeySmokeStep(CefRefPtr<CefBrowser> browser, const std::string& event);
  void SendTestKey(CefRefPtr<CefBrowser> browser,
                   TestKey key,
                   TestKeyPhase phase = TestKeyPhase::kPress);
  // Key smoke: holds `key` for `repeats` OS repeats 50 ms apart, then
  // releases it unless `release` is false, then runs `probe` in the page.
  void SendTestHold(CefRefPtr<CefBrowser> browser,
                    TestKey key,
                    int repeats,
                    bool release,
                    int64_t probe_delay_ms,
                    const std::string& probe);
  void RunTestScript(CefRefPtr<CefBrowser> browser, const std::string& script);
  // Starts, keeps alive, or stops held-key scrolling for a key that completed
  // a scroll binding and is held down.
  void HandleHold(CefRefPtr<CefBrowser> browser,
                  const KeyDispatch& dispatch,
                  int native_key_code);
  // Ends a held-key scroll, if one runs. Safe to call at any time.
  void StopHeldScroll();
  void RunBinding(CefRefPtr<CefBrowser> browser, const KeyDispatch& dispatch);
  void OnSmokeTimeout();
  void FinishSmoke(int code);

  const bool alloy_style_;
  // Page a new tab opens: the initial URL until there is a home page setting.
  const std::string new_tab_url_;
  const bool smoke_test_;
  const bool tab_smoke_test_;
  const bool key_smoke_test_;
  // Key smoke: index of the step waiting for its event. See KeySmokeStep.
  int key_smoke_step_ = 0;
  // Tab smoke stage. See TabSmokeLoadEnd.
  int tab_smoke_stage_ = 0;
  TabId tab_smoke_first_ = 0;
  TabId tab_smoke_second_ = 0;
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
  // Set only while RunBinding runs a command. UI thread only.
  CefRefPtr<CefBrowser> key_target_;
  // A held scroll key. One keyboard, so at most one. UI thread only.
  struct HeldScroll {
    TabId tab;
    CefRefPtr<CefFrame> frame;
    int native_key_code;
    std::chrono::steady_clock::time_point last_keep_alive;
  };
  std::optional<HeldScroll> held_scroll_;
  // UI thread only.
  CefRefPtr<CefTextfield> status_bar_;
  std::set<TabId> loading_tabs_;

  IMPLEMENT_REFCOUNTING(CefBrowserClient);
};

}  // namespace vrowser
