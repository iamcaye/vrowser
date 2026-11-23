// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_renderer_app.hpp"

#include "browser/cef/hint_script.hpp"
#include "include/cef_process_message.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_helpers.h"

namespace vrowser {

void CefRendererApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefDOMNode> node) {
  // `frame` and `node` are empty when focus leaves every node. The report then
  // belongs to this renderer's focused frame, which in an out-of-process
  // iframe is not the main frame.
  CefRefPtr<CefFrame> target = frame ? frame : browser->GetFocusedFrame();
  if (!target) {
    target = browser->GetMainFrame();
  }
  if (!target) {
    return;
  }
  CefRefPtr<CefProcessMessage> message =
      CefProcessMessage::Create(kEditableFocusMessage);
  message->GetArgumentList()->SetBool(0, node && node->IsEditable());
  target->SendProcessMessage(PID_BROWSER, message);
}

bool CefRendererApp::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (source_process != PID_BROWSER || !frame) {
    return false;
  }
  const std::string name = message->GetName().ToString();
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  const int request = args->GetInt(0);
  const std::string frame_id = frame->GetIdentifier().ToString();
  if (name == kHintCollectMessage) {
    CollectHints(frame, request);
    return true;
  }
  if (name == kHintShowMessage) {
    CefRefPtr<CefListValue> labels = args->GetList(1);
    CallHints(frame, request, "show", [labels] {
      const int count = labels ? static_cast<int>(labels->GetSize()) : 0;
      CefRefPtr<CefV8Value> array = CefV8Value::CreateArray(count);
      for (int i = 0; i < count; ++i) {
        array->SetValue(i, CefV8Value::CreateString(
                               labels->GetString(static_cast<size_t>(i))));
      }
      return CefV8ValueList{array};
    });
    return true;
  }
  if (name == kHintFilterMessage) {
    const CefString typed = args->GetString(1);
    CallHints(frame, request, "filter", [typed] {
      return CefV8ValueList{CefV8Value::CreateString(typed)};
    });
    return true;
  }
  if (name == kHintActivateMessage) {
    const int index = args->GetInt(1);
    CallHints(frame, request, "activate",
              [index] { return CefV8ValueList{CefV8Value::CreateInt(index)}; });
    hint_frames_.erase(frame_id);
    return true;
  }
  if (name == kHintClearMessage) {
    CallHints(frame, request, "clear", [] { return CefV8ValueList{}; });
    hint_frames_.erase(frame_id);
    return true;
  }
  return false;
}

void CefRendererApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       CefRefPtr<CefV8Context> context) {
  CEF_REQUIRE_RENDERER_THREAD();
  if (frame) {
    hint_frames_.erase(frame->GetIdentifier().ToString());
  }
}

void CefRendererApp::CollectHints(CefRefPtr<CefFrame> frame, int request) {
  const std::string frame_id = frame->GetIdentifier().ToString();
  hint_frames_.erase(frame_id);
  CefRefPtr<CefListValue> rects = CefListValue::Create();
  CefRefPtr<CefV8Context> context = frame->GetV8Context();
  if (context && context->IsValid() && context->Enter()) {
    CefRefPtr<CefV8Value> controller;
    CefRefPtr<CefV8Exception> exception;
    if (context->Eval(kHintControllerScript, CefString(), 0, controller,
                      exception) &&
        controller && controller->IsObject()) {
      CefRefPtr<CefV8Value> numbers = controller->GetValue("rects");
      if (numbers && numbers->IsArray()) {
        const int length = numbers->GetArrayLength();
        // Whole rects only; a page that broke the array gets fewer hints.
        for (int i = 0; i + 3 < length; i += 4) {
          bool ok = true;
          double values[4];
          for (int j = 0; j < 4; ++j) {
            CefRefPtr<CefV8Value> value = numbers->GetValue(i + j);
            ok = ok && value && value->IsDouble();
            values[j] = ok ? value->GetDoubleValue() : 0;
          }
          if (!ok) {
            break;
          }
          for (double value : values) {
            rects->SetDouble(rects->GetSize(), value);
          }
        }
      }
      hint_frames_[frame_id] = HintFrame{request, context, controller};
    }
    context->Exit();
  }
  CefRefPtr<CefProcessMessage> reply =
      CefProcessMessage::Create(kHintRectsMessage);
  reply->GetArgumentList()->SetInt(0, request);
  reply->GetArgumentList()->SetList(1, rects);
  frame->SendProcessMessage(PID_BROWSER, reply);
}

void CefRendererApp::CallHints(
    CefRefPtr<CefFrame> frame,
    int request,
    const char* method,
    const std::function<CefV8ValueList()>& make_args) {
  const auto it = hint_frames_.find(frame->GetIdentifier().ToString());
  if (it == hint_frames_.end() || it->second.request != request) {
    return;
  }
  HintFrame& hints = it->second;
  if (!hints.context->IsValid() || !hints.context->Enter()) {
    return;
  }
  CefRefPtr<CefV8Value> function = hints.controller->GetValue(method);
  if (function && function->IsFunction()) {
    function->ExecuteFunction(hints.controller, make_args());
  }
  hints.context->Exit();
}

}  // namespace vrowser
