// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "include/cef_app.h"

namespace vrowser {

// Renderer to browser message: argument 0 is a bool, true when the newly
// focused node is editable. The browser treats it as untrusted input that can
// only switch that tab between Normal and Insert.
inline constexpr char kEditableFocusMessage[] = "vrowser.editable-focus";

// CefApp for subprocesses. Only the renderer uses it: it reports editable
// focus changes so the browser can select Insert mode.
class CefRendererApp : public CefApp, public CefRenderProcessHandler {
 public:
  CefRendererApp() = default;

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefDOMNode> node) override;

 private:
  IMPLEMENT_REFCOUNTING(CefRendererApp);
};

}  // namespace vrowser
