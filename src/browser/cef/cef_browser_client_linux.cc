// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#include <string>

#include "include/cef_browser.h"

#if defined(CEF_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace vrowser {

void CefBrowserClient::PlatformTitleChange(CefRefPtr<CefBrowser> browser,
                                           const CefString& title) {
#if defined(CEF_X11)
  const std::string utf8 = title.ToString();
  ::Display* display = cef_get_xdisplay();
  if (display == nullptr) {
    return;
  }
  ::Window window = browser->GetHost()->GetWindowHandle();
  if (window == kNullWindowHandle) {
    return;
  }
  const char* names[] = {"_NET_WM_NAME", "UTF8_STRING"};
  Atom atoms[2];
  if (XInternAtoms(display, const_cast<char**>(names), 2, False, atoms) == 0) {
    return;
  }
  XChangeProperty(display, window, atoms[0], atoms[1], 8, PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8.data()),
                  static_cast<int>(utf8.size()));
  XStoreName(display, window, utf8.c_str());
#else
  (void)browser;
  (void)title;
#endif
}

void CefBrowserClient::PlatformShowWindow(CefRefPtr<CefBrowser> browser) {
  (void)browser;
}

}  // namespace vrowser
