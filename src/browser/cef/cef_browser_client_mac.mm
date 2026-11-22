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

void CefBrowserClient::PlatformSendTestKey(CefRefPtr<CefBrowser> browser,
                                           const TestKey& key) {
  NSWindow* window = WindowForBrowser(browser);
  if (window == nil) {
    return;
  }
  const unichar unit = key.character;
  const unichar unshifted =
      key.shift && unit >= 'A' && unit <= 'Z' ? unit - 'A' + 'a' : unit;
  NSString* characters = [NSString stringWithCharacters:&unit length:1];
  NSString* ignoring = [NSString stringWithCharacters:&unshifted length:1];
  const NSEventModifierFlags flags = key.shift ? NSEventModifierFlagShift : 0;
  for (const NSEventType type : {NSEventTypeKeyDown, NSEventTypeKeyUp}) {
    NSEvent* event = [NSEvent
                   keyEventWithType:type
                           location:NSZeroPoint
                      modifierFlags:flags
                          timestamp:[[NSProcessInfo processInfo] systemUptime]
                       windowNumber:[window windowNumber]
                            context:nil
                         characters:characters
        charactersIgnoringModifiers:ignoring
                          isARepeat:NO
                            keyCode:static_cast<unsigned short>(
                                        key.mac_key_code)];
    [window sendEvent:event];
  }
}

}  // namespace vrowser
