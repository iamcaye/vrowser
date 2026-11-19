// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_runtime_app.hpp"
#include "include/base/cef_build.h"
#include "include/cef_app.h"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#if defined(CEF_X11)
namespace {

int XErrorHandlerImpl(Display* display, XErrorEvent* event) {
  (void)display;
  (void)event;
  return 0;
}

int XIOErrorHandlerImpl(Display* display) {
  (void)display;
  return 0;
}

}  // namespace
#endif

NO_STACK_PROTECTOR int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  const int subprocess = CefExecuteProcess(main_args, nullptr, nullptr);
  if (subprocess >= 0) {
    return subprocess;
  }

#if defined(CEF_X11)
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  const vrowser::StartupConfig config = vrowser::ParseStartupConfig(argc, argv);
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

  CefRunMessageLoop();
  const int exit_code = app->exit_code();
  CefShutdown();
  return exit_code;
}
