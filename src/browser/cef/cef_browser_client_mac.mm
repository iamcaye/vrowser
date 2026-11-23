// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#import <Cocoa/Cocoa.h>

#include "include/cef_browser.h"

namespace vrowser {
namespace {

// The key-up monitor installed by PlatformWatchKeyUps. Main (UI) thread.
id g_key_up_monitor = nil;

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

void CefBrowserClient::PlatformWatchKeyUps(bool watch) {
  if (watch && g_key_up_monitor == nil) {
    // Local monitors run on the main thread, which is the CEF UI thread on
    // macOS, before the event reaches a window. The event is passed on
    // unchanged.
    g_key_up_monitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
                                     handler:^NSEvent*(NSEvent* event) {
                                       if (CefBrowserClient* client =
                                               CefBrowserClient::
                                                   GetInstance()) {
                                         client->OnNativeKeyUp([event keyCode]);
                                       }
                                       return event;
                                     }];
  } else if (!watch && g_key_up_monitor != nil) {
    [NSEvent removeMonitor:g_key_up_monitor];
    g_key_up_monitor = nil;
  }
}

void CefBrowserClient::PlatformSendTestKey(CefRefPtr<CefBrowser> browser,
                                           const TestKey& key,
                                           TestKeyPhase phase) {
  NSWindow* window = WindowForBrowser(browser);
  if (window == nil) {
    return;
  }
  const unichar unit = key.character;
  const unichar unshifted = key.unshifted != 0 ? key.unshifted
                            : key.shift && unit >= 'A' && unit <= 'Z'
                                ? unit - 'A' + 'a'
                                : unit;
  NSString* characters = [NSString stringWithCharacters:&unit length:1];
  NSString* ignoring = [NSString stringWithCharacters:&unshifted length:1];
  const NSEventModifierFlags flags = key.shift ? NSEventModifierFlagShift : 0;
  for (const NSEventType type : {NSEventTypeKeyDown, NSEventTypeKeyUp}) {
    const bool down = type == NSEventTypeKeyDown;
    if ((down && phase == TestKeyPhase::kUp) ||
        (!down &&
         (phase == TestKeyPhase::kDown || phase == TestKeyPhase::kRepeat))) {
      continue;
    }
    NSEvent* event = [NSEvent
                   keyEventWithType:type
                           location:NSZeroPoint
                      modifierFlags:flags
                          timestamp:[[NSProcessInfo processInfo] systemUptime]
                       windowNumber:[window windowNumber]
                            context:nil
                         characters:characters
        charactersIgnoringModifiers:ignoring
                          isARepeat:phase == TestKeyPhase::kRepeat
                            keyCode:static_cast<unsigned short>(
                                        key.mac_key_code)];
    if (phase == TestKeyPhase::kUp) {
      // Through the app queue, like a typed key-up, so the key-up monitor
      // sees it before the window does.
      [NSApp postEvent:event atStart:NO];
    } else {
      [window sendEvent:event];
    }
  }
}

}  // namespace vrowser
