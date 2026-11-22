// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_renderer_app.hpp"

#include "include/cef_process_message.h"

namespace vrowser {

void CefRendererApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefDOMNode> node) {
  // `frame` and `node` are empty when focus leaves every node.
  CefRefPtr<CefFrame> target = frame ? frame : browser->GetMainFrame();
  if (!target) {
    return;
  }
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kEditableFocusMessage);
  message->GetArgumentList()->SetBool(0, node && node->IsEditable());
  target->SendProcessMessage(PID_BROWSER, message);
}

}  // namespace vrowser
