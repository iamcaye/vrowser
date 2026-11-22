// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/views_delegates.hpp"

#include <utility>

#include "browser/cef/cef_browser_client.hpp"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"

namespace vrowser {
namespace {

class WindowDelegate : public CefWindowDelegate {
 public:
  WindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                 cef_runtime_style_t runtime_style,
                 bool status_bar)
      : initial_view_(std::move(browser_view)),
        runtime_style_(runtime_style),
        status_bar_(status_bar) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    if (status_bar_) {
      AddTabsAndStatusBar(window);
    } else {
      window->SetToFillLayout();
      window->AddChildView(initial_view_);
    }
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
  void AddTabsAndStatusBar(CefRefPtr<CefWindow> window) {
    CefBoxLayoutSettings settings;
    settings.horizontal = false;
    CefRefPtr<CefBoxLayout> layout = window->SetToBoxLayout(settings);

    CefRefPtr<CefPanel> tabs = CefPanel::CreatePanel(nullptr);
    tabs->SetToFillLayout();
    tabs->AddChildView(initial_view_);
    window->AddChildView(tabs);
    layout->SetFlexForView(tabs, 1);

    // Read-only and not focusable: keys stay with the page and the shell.
    CefRefPtr<CefTextfield> bar = CefTextfield::CreateTextfield(nullptr);
    bar->SetReadOnly(true);
    bar->SetFocusable(false);
    bar->SetFontList("Menlo, Consolas, DejaVu Sans Mono, 12px");
    window->AddChildView(bar);
    layout->SetFlexForView(bar, 0);
    if (CefBrowserClient* client = CefBrowserClient::GetInstance()) {
      client->AttachStatusBar(bar);
    }
  }

  CefRefPtr<CefBrowserView> initial_view_;
  const cef_runtime_style_t runtime_style_;
  const bool status_bar_;

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
    cef_runtime_style_t runtime_style,
    bool status_bar) {
  return new WindowDelegate(std::move(browser_view), runtime_style, status_bar);
}

CefRefPtr<CefBrowserViewDelegate> CreateBrowserViewDelegate(
    cef_runtime_style_t runtime_style) {
  return new BrowserViewDelegate(runtime_style);
}

}  // namespace vrowser
