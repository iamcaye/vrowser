// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#import <Cocoa/Cocoa.h>

#include "browser/cef/cef_browser_client.hpp"
#include "browser/cef/cef_runtime_app.hpp"
#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"

@interface VrowserApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handling_send_event_;
}
@end

@interface VrowserAppDelegate : NSObject <NSApplicationDelegate>
- (void)createApplication:(id)object;
- (void)tryToTerminateApplication:(NSApplication*)app;
@end

@implementation VrowserApplication

- (BOOL)isHandlingSendEvent {
  return handling_send_event_;
}

- (void)setHandlingSendEvent:(BOOL)handling_send_event {
  handling_send_event_ = handling_send_event;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sending_event_scope;
  [super sendEvent:event];
}

- (void)terminate:(id)sender {
  VrowserAppDelegate* delegate =
      static_cast<VrowserAppDelegate*>([NSApp delegate]);
  [delegate tryToTerminateApplication:self];
}

@end

@implementation VrowserAppDelegate

- (void)createApplication:(id)object {
  NSMenu* menu_bar = [[NSMenu alloc] init];
  NSMenuItem* app_item = [[NSMenuItem alloc] init];
  [menu_bar addItem:app_item];
  NSMenu* app_menu = [[NSMenu alloc] initWithTitle:@"Vrowser"];
  NSMenuItem* quit_item =
      [[NSMenuItem alloc] initWithTitle:@"Quit Vrowser"
                                 action:@selector(terminate:)
                          keyEquivalent:@"q"];
  [app_menu addItem:quit_item];
  [app_item setSubmenu:app_menu];
  [NSApp setMainMenu:menu_bar];
  [NSApp setDelegate:self];
}

- (void)tryToTerminateApplication:(NSApplication*)app {
  vrowser::CefBrowserClient* client = vrowser::CefBrowserClient::GetInstance();
  if (client != nullptr && !client->IsClosing()) {
    client->CloseAllBrowsers(false);
  }
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)app {
  return NSTerminateNow;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)app
                    hasVisibleWindows:(BOOL)flag {
  vrowser::CefBrowserClient* client = vrowser::CefBrowserClient::GetInstance();
  if (client != nullptr && !client->IsClosing()) {
    client->ShowMainWindow();
  }
  return NO;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}

@end

int main(int argc, char* argv[]) {
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInMain()) {
    return 1;
  }

  const vrowser::StartupConfig config = vrowser::ParseStartupConfig(argc, argv);
  CefMainArgs main_args(argc, argv);

  @autoreleasepool {
    [VrowserApplication sharedApplication];
    CHECK([NSApp isKindOfClass:[VrowserApplication class]]);

    CefSettings settings;
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = 1;
#endif
    if (!vrowser::ConfigureCefSettings(settings, config)) {
      return 1;
    }

    CefRefPtr<vrowser::CefRuntimeApp> app(new vrowser::CefRuntimeApp(config));
    if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
      return CefGetExitCode();
    }

    VrowserAppDelegate* delegate = [[VrowserAppDelegate alloc] init];
    NSApp.delegate = delegate;
    [delegate performSelectorOnMainThread:@selector(createApplication:)
                               withObject:nil
                            waitUntilDone:NO];

    CefRunMessageLoop();
    const int exit_code = app->exit_code();
    CefShutdown();
    NSApp.delegate = nil;
    return exit_code;
  }
}
