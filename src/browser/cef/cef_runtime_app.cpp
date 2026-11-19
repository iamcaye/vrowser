// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_runtime_app.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include "browser/cef/cef_browser_client.hpp"
#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace vrowser {
namespace {

std::string PathToUtf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  std::string out;
  out.resize(utf8.size());
  for (size_t i = 0; i < utf8.size(); ++i) {
    out[i] = static_cast<char>(utf8[i]);
  }
  return out;
}

class WindowDelegate : public CefWindowDelegate {
 public:
  WindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                 cef_runtime_style_t runtime_style)
      : browser_view_(std::move(browser_view)), runtime_style_(runtime_style) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->SetTitle("Vrowser");
    window->Show();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (!browser) {
      return true;
    }
    // A close that has already started must be allowed to finish. Returning
    // false here leaves the browser half-closed and the message loop running.
    if (browser->GetHost()->IsReadyToBeClosed()) {
      return true;
    }
    return browser->GetHost()->TryCloseBrowser();
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(1280, 800);
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return runtime_style_;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;
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
        new WindowDelegate(popup_browser_view, runtime_style_));
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

bool ConfigureCefSettings(CefSettings& settings, const StartupConfig& config) {
  std::error_code error;
  const std::filesystem::path root =
      std::filesystem::absolute(config.user_data_dir, error);
  if (error) {
    std::fprintf(stderr, "vrowser: cannot resolve user data directory: %s\n",
                 error.message().c_str());
    return false;
  }
  std::filesystem::create_directories(root, error);
  if (error) {
    std::fprintf(stderr, "vrowser: cannot create %s: %s\n",
                 PathToUtf8(root).c_str(), error.message().c_str());
    return false;
  }

  const std::string root_utf8 = PathToUtf8(root);
  CefString(&settings.root_cache_path).FromString(root_utf8);
  CefString(&settings.cache_path).FromString(root_utf8);
  CefString(&settings.log_file).FromString(PathToUtf8(root / "vrowser.log"));
  settings.log_severity = LOGSEVERITY_WARNING;
  return true;
}

CefRuntimeApp::CefRuntimeApp(StartupConfig config)
    : config_(std::move(config)),
      client_(new CefBrowserClient(config_.alloy_style, config_.smoke_test)) {}

void CefRuntimeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  const cef_runtime_style_t runtime_style =
      config_.alloy_style ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;
  CefBrowserSettings browser_settings;
  std::fprintf(stderr, "vrowser: opening %s\n", config_.initial_url.c_str());

  if (config_.use_views) {
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client_, config_.initial_url, browser_settings, nullptr, nullptr,
        new BrowserViewDelegate(runtime_style));
    CefWindow::CreateTopLevelWindow(
        new WindowDelegate(browser_view, runtime_style));
    return;
  }

  CefWindowInfo window_info;
#if defined(OS_WIN)
  window_info.SetAsPopup(nullptr, "Vrowser");
#endif
  window_info.runtime_style = runtime_style;
  CefBrowserHost::CreateBrowser(window_info, client_, config_.initial_url,
                                browser_settings, nullptr, nullptr);
}

CefRefPtr<CefClient> CefRuntimeApp::GetDefaultClient() {
  return client_;
}

bool CefRuntimeApp::OnAlreadyRunningAppRelaunch(
    CefRefPtr<CefCommandLine> command_line,
    const CefString& current_directory) {
  CEF_REQUIRE_UI_THREAD();
  if (client_) {
    client_->HandleRelaunch(command_line->GetSwitchValue("url").ToString());
  }
  return true;
}

int CefRuntimeApp::exit_code() const {
  return client_ ? client_->exit_code() : 1;
}

}  // namespace vrowser
