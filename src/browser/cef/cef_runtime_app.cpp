// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_runtime_app.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include "browser/cef/cef_browser_client.hpp"
#include "browser/cef/views_delegates.hpp"
#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"

namespace vrowser {

CefRuntimeApp::~CefRuntimeApp() = default;

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
    : config_(std::move(config)), client_(new CefBrowserClient(config_)) {}

void CefRuntimeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  const cef_runtime_style_t runtime_style =
      config_.alloy_style ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;
  CefBrowserSettings browser_settings;
  if (config_.key_smoke_test) {
    config_.initial_url = CefBrowserClient::KeySmokePageUrl();
  }
  std::fprintf(stderr, "vrowser: opening %s\n", config_.initial_url.c_str());

  if (config_.use_views) {
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client_, config_.initial_url, browser_settings, nullptr, nullptr,
        CreateBrowserViewDelegate(runtime_style));
    CefWindow::CreateTopLevelWindow(
        CreateWindowDelegate(browser_view, runtime_style));
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
