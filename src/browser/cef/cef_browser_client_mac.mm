// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#import <Cocoa/Cocoa.h>

#include "include/cef_browser.h"

namespace vrowser {
namespace {

NSWindow* WindowForBrowser(CefRefPtr<CefBrowser> browser) {
  NSView* view =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(browser->GetHost()->GetWindowHandle());
  return [view window];
}

}  // namespace

void CefBrowserClient::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                           const CefString& title) {
  NSWindow* window = WindowForBrowser(browser);
  const std::string utf8 = title.ToString();
  NSString* text = [NSString stringWithUTF8String:utf8.c_str()];
  if (text != nil) {
    [window setTitle:text];
  }
}

void CefBrowserClient::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  NSWindow* window = WindowForBrowser(browser);
  [window makeKeyAndOrderFront:nil];
}

}  // namespace vrowser
