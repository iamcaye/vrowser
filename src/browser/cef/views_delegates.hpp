// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"

namespace vrowser {

// Delegate for a top-level window that starts with `browser_view` and can
// host more browser views (tabs) under a fill layout. Close requests are
// routed to CefBrowserClient so every browser in the window closes cleanly.
CefRefPtr<CefWindowDelegate> CreateWindowDelegate(
    CefRefPtr<CefBrowserView> browser_view,
    cef_runtime_style_t runtime_style);

// Delegate for browser views. Popups open in their own top-level window.
CefRefPtr<CefBrowserViewDelegate> CreateBrowserViewDelegate(
    cef_runtime_style_t runtime_style);

}  // namespace vrowser
