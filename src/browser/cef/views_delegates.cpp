// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/views_delegates.hpp"

#include <utility>

#include "browser/cef/cef_browser_client.hpp"
#include "include/views/cef_fill_layout.h"

namespace vrowser {
namespace {

class WindowDelegate : public CefWindowDelegate {
 public:
  WindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                 cef_runtime_style_t runtime_style)
      : initial_view_(std::move(browser_view)), runtime_style_(runtime_style) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->SetToFillLayout();
    window->AddChildView(initial_view_);
    // The window owns its child views. Keeping a reference here would keep a
    // closed tab's view, and its browser, alive.
    initial_view_ = nullptr;
    window->SetTitle("Vrowser");
    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefBrowserClient* client = CefBrowserClient::GetInstance();
    return client == nullptr || client->CanCloseWindow(window);
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(1280, 800);
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> initial_view_;
  const cef_runtime_style_t runtime_style_;

  IMPLEMENT_REFCOUNTING(WindowDelegate);
};

class BrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit BrowserViewDelegate(cef_runtime_style_t runtime_style)
      : runtime_style_(runtime_style) {}

  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override {
    CefWindow::CreateTopLevelWindow(
        CreateWindowDelegate(popup_browser_view, runtime_style_));
    return true;
  }

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  const cef_runtime_style_t runtime_style_;

  IMPLEMENT_REFCOUNTING(BrowserViewDelegate);
};

}  // namespace

CefRefPtr<CefWindowDelegate> CreateWindowDelegate(
    CefRefPtr<CefBrowserView> browser_view,
    cef_runtime_style_t runtime_style) {
  return new WindowDelegate(std::move(browser_view), runtime_style);
}

CefRefPtr<CefBrowserViewDelegate> CreateBrowserViewDelegate(
    cef_runtime_style_t runtime_style) {
  return new BrowserViewDelegate(runtime_style);
}

}  // namespace vrowser
