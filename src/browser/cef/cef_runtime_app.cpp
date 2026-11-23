// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_runtime_app.hpp"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "browser/cef/cef_browser_client.hpp"
#include "browser/cef/views_delegates.hpp"
#include "vrowser/core/profile.hpp"
#include "include/cef_browser.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
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
  const std::optional<std::filesystem::path> profile =
      ProfileDirectory(root, config.profile);
  if (!profile) {
    std::fprintf(stderr,
                 "vrowser: invalid --profile \"%s\": use 1-64 of a-z, 0-9, "
                 "_ and -\n",
                 config.profile.c_str());
    return false;
  }
  std::filesystem::create_directories(*profile, error);
  if (error) {
    std::fprintf(stderr, "vrowser: cannot create %s: %s\n",
                 PathToUtf8(*profile).c_str(), error.message().c_str());
    return false;
  }

  // The root is the process singleton key: one process per root, whatever
  // the profile. The global context uses the selected profile, so nothing is
  // written to a profile that was not chosen.
  CefString(&settings.root_cache_path).FromString(PathToUtf8(root));
  CefString(&settings.cache_path).FromString(PathToUtf8(*profile));
  CefString(&settings.log_file).FromString(PathToUtf8(root / "vrowser.log"));
  settings.log_severity = LOGSEVERITY_WARNING;
  return true;
}

namespace {

// Opens the first window once the profile's request context is ready.
class ProfileContextHandler : public CefRequestContextHandler {
 public:
  explicit ProfileContextHandler(CefRefPtr<CefRuntimeApp> app) : app_(app) {}

  void OnRequestContextInitialized(
      CefRefPtr<CefRequestContext> request_context) override {
    CEF_REQUIRE_UI_THREAD();
    // CEF keeps the handler until the context is gone; drop the app now.
    if (CefRefPtr<CefRuntimeApp> app = std::move(app_)) {
      app->OpenFirstWindow(request_context);
    }
  }

 private:
  CefRefPtr<CefRuntimeApp> app_;

  IMPLEMENT_REFCOUNTING(ProfileContextHandler);
};

}  // namespace

CefRuntimeApp::CefRuntimeApp(StartupConfig config)
    : config_(std::move(config)), client_(new CefBrowserClient(config_)) {}

void CefRuntimeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();
  // One request context per profile directory. Every tab of this window
  // uses it, so cookies and storage stay in the profile's directory. It
  // initializes asynchronously; the window opens once it is ready.
  std::error_code error;
  const std::optional<std::filesystem::path> profile = ProfileDirectory(
      std::filesystem::absolute(config_.user_data_dir, error), config_.profile);
  if (!profile || error) {
    // ConfigureCefSettings validated the profile; this is not expected.
    std::fprintf(stderr, "vrowser: cannot resolve the profile directory\n");
    CefQuitMessageLoop();
    return;
  }
  CefRequestContextSettings context_settings;
  CefString(&context_settings.cache_path).FromString(PathToUtf8(*profile));
  CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(
      context_settings, new ProfileContextHandler(this));
  // The handler may already have run; then the client holds the context.
  if (!first_window_opened_) {
    request_context_ = context;
  }
}

void CefRuntimeApp::OpenFirstWindow(
    CefRefPtr<CefRequestContext> request_context) {
  CEF_REQUIRE_UI_THREAD();
  // The client keeps the context for new tabs and releases it on the last
  // close; the app holds no reference past this point.
  first_window_opened_ = true;
  request_context_ = nullptr;
  std::fprintf(stderr, "vrowser: profile %s\n", config_.profile.c_str());
  client_->SetRequestContext(request_context);
  const cef_runtime_style_t runtime_style =
      config_.alloy_style ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;
  CefBrowserSettings browser_settings;
  if (config_.key_smoke_test) {
    config_.initial_url = CefBrowserClient::KeySmokePageUrl();
  }
  std::fprintf(stderr, "vrowser: opening %s\n", config_.initial_url.c_str());

  if (config_.use_views) {
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client_, config_.initial_url, browser_settings, nullptr,
        request_context, CreateBrowserViewDelegate(runtime_style));
    CefWindow::CreateTopLevelWindow(
        // Alloy has no toolbar, so the main window gets a status bar with the
        // URL. Chrome style keeps Chromium's toolbar.
        CreateWindowDelegate(browser_view, runtime_style, config_.alloy_style));
    return;
  }

  CefWindowInfo window_info;
#if defined(OS_WIN)
  window_info.SetAsPopup(nullptr, "Vrowser");
#endif
  window_info.runtime_style = runtime_style;
  CefBrowserHost::CreateBrowser(window_info, client_, config_.initial_url,
                                browser_settings, nullptr, request_context);
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
