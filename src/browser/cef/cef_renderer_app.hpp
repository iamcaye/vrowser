// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>
#include <map>
#include <string>

#include "include/cef_app.h"

namespace vrowser {

// Renderer to browser message: argument 0 is a bool, true when the newly
// focused node is editable. The browser treats it as untrusted input that can
// only switch that tab between Normal and Insert.
inline constexpr char kEditableFocusMessage[] = "vrowser.editable-focus";

// Link hints. Browser to renderer, sent to the frame being hinted; argument 0
// is the request ID:
//   collect            find hintable elements; answered by kHintRectsMessage
//   show  (1: labels)  draw labels, one string per rect
//   filter(1: typed)   keep labels that start with the typed text
//   activate(1: index) remove the labels and click or focus that element
//   clear              remove the labels
// Renderer to browser: kHintRectsMessage (0: request ID, 1: list of
// x, y, width, height doubles in CSS pixels of the frame's viewport).
inline constexpr char kHintCollectMessage[] = "vrowser.hint-collect";
inline constexpr char kHintShowMessage[] = "vrowser.hint-show";
inline constexpr char kHintFilterMessage[] = "vrowser.hint-filter";
inline constexpr char kHintActivateMessage[] = "vrowser.hint-activate";
inline constexpr char kHintClearMessage[] = "vrowser.hint-clear";
inline constexpr char kHintRectsMessage[] = "vrowser.hint-rects";

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

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;

 private:
  // The hint controller of one frame, from its last collect.
  struct HintFrame {
    int request = 0;
    CefRefPtr<CefV8Context> context;
    CefRefPtr<CefV8Value> controller;
  };

  void CollectHints(CefRefPtr<CefFrame> frame, int request);
  // Calls `method` on the frame's controller when `request` is current. The
  // arguments are built by `make_args` inside the frame's V8 context.
  void CallHints(CefRefPtr<CefFrame> frame,
                 int request,
                 const char* method,
                 const std::function<CefV8ValueList()>& make_args);

  // By frame identifier. Renderer main thread only.
  std::map<std::string, HintFrame> hint_frames_;

  IMPLEMENT_REFCOUNTING(CefRendererApp);
};

}  // namespace vrowser
