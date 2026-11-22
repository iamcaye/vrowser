// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#include <windows.h>

#include <string>

#include "include/cef_browser.h"

namespace vrowser {

void CefBrowserClient::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                           const CefString& title) {
  const HWND window = browser->GetHost()->GetWindowHandle();
  if (window != nullptr) {
    const std::wstring text = title.ToWString();
    SetWindowTextW(window, text.c_str());
  }
}

void CefBrowserClient::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  const HWND window = browser->GetHost()->GetWindowHandle();
  if (window != nullptr) {
    ShowWindow(window, SW_SHOWNORMAL);
    SetForegroundWindow(window);
  }
}

void CefBrowserClient::PlatformWatchKeyUps(bool watch) {
  // Key-ups of consumed key-downs reach OnPreKeyEvent here.
  (void)watch;
}

}  // namespace vrowser
