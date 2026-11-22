// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"

namespace vrowser {

// Delegate for a top-level window that starts with `browser_view` and can
// host more browser views (tabs) under a fill layout. Close requests are
// routed to CefBrowserClient so every browser in the window closes cleanly.
//
// With `status_bar`, the tabs go in a panel above a one-line, read-only
// status bar, which is handed to CefBrowserClient::AttachStatusBar. Tab views
// are always children of one panel (the window itself without a status bar),
// so callers add and remove them through the view's parent.
CefRefPtr<CefWindowDelegate> CreateWindowDelegate(
    CefRefPtr<CefBrowserView> browser_view,
    cef_runtime_style_t runtime_style,
    bool status_bar = false);

// Delegate for browser views. Popups open in their own top-level window.
CefRefPtr<CefBrowserViewDelegate> CreateBrowserViewDelegate(
    cef_runtime_style_t runtime_style);

}  // namespace vrowser
