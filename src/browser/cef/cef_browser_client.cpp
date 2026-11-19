// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#include <cstdio>
#include <sstream>
#include <utility>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace vrowser {
namespace {

CefBrowserClient* g_instance = nullptr;

std::string DataUri(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

CefBrowserClient::CefBrowserClient(bool alloy_style, bool smoke_test)
    : alloy_style_(alloy_style),
      smoke_test_(smoke_test),
      exit_code_(smoke_test ? 2 : 0) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
}

CefBrowserClient::~CefBrowserClient() {
  g_instance = nullptr;
}

CefBrowserClient* CefBrowserClient::GetInstance() {
  return g_instance;
}

void CefBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                     const CefString& title) {
  CEF_REQUIRE_UI_THREAD();
  if (CefRefPtr<CefBrowserView> browser_view =
          CefBrowserView::GetForBrowser(browser)) {
    if (CefRefPtr<CefWindow> window = browser_view->GetWindow()) {
      window->SetTitle(title);
    }
  } else if (alloy_style_) {
    PlatformTitleChange(browser, title);
  }
}

void CefBrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  CHECK_EQ(alloy_style_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME,
           browser->GetHost()->GetRuntimeStyle());
  browsers_.push_back(browser);

  if (smoke_test_ && !smoke_timer_armed_) {
    smoke_timer_armed_ = true;
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&CefBrowserClient::OnSmokeTimeout, this), 45000);
  }
}

bool CefBrowserClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (browsers_.size() == 1) {
    is_closing_ = true;
  }
  return false;
}

void CefBrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  for (BrowserList::iterator it = browsers_.begin(); it != browsers_.end();
       ++it) {
    if ((*it)->IsSame(browser)) {
      browsers_.erase(it);
      break;
    }
  }
  if (browsers_.empty()) {
    CefQuitMessageLoop();
  }
}

void CefBrowserClient::OnLoadStart(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefLoadHandler::TransitionType transition_type) {
  CEF_REQUIRE_UI_THREAD();
  (void)browser;
  (void)transition_type;
  if (frame->IsMain()) {
    std::fprintf(stderr, "vrowser: load-start %s\n",
                 frame->GetURL().ToString().c_str());
  }
}

void CefBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 int http_status_code) {
  CEF_REQUIRE_UI_THREAD();
  if (!frame->IsMain()) {
    return;
  }
  std::fprintf(stderr, "vrowser: load-end %d %s\n", http_status_code,
               frame->GetURL().ToString().c_str());
  if (smoke_test_ && http_status_code >= 200 && http_status_code < 400) {
    std::fprintf(stderr, "vrowser-smoke: ok %d %s\n", http_status_code,
                 frame->GetURL().ToString().c_str());
    FinishSmoke(0);
  }
}

void CefBrowserClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   ErrorCode error_code,
                                   const CefString& error_text,
                                   const CefString& failed_url) {
  CEF_REQUIRE_UI_THREAD();
  if (error_code == ERR_ABORTED || !frame->IsMain()) {
    return;
  }
  if (smoke_test_) {
    std::fprintf(stderr, "vrowser-smoke: error %d %s\n",
                 static_cast<int>(error_code), failed_url.ToString().c_str());
    FinishSmoke(1);
    return;
  }
  if (!alloy_style_) {
    return;
  }

  std::stringstream html;
  html << "<html><body style=\"font-family: sans-serif\">"
          "<h2>Failed to load URL "
       << failed_url.ToString() << "</h2><p>" << error_text.ToString() << " ("
       << static_cast<int>(error_code) << ")</p></body></html>";
  frame->LoadURL(DataUri(html.str(), "text/html"));
}

void CefBrowserClient::ShowMainWindow() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&CefBrowserClient::ShowMainWindow, this));
    return;
  }
  if (browsers_.empty()) {
    return;
  }
  CefRefPtr<CefBrowser> browser = browsers_.front();
  if (CefRefPtr<CefBrowserView> browser_view =
          CefBrowserView::GetForBrowser(browser)) {
    if (CefRefPtr<CefWindow> window = browser_view->GetWindow()) {
      window->Show();
    }
  } else if (alloy_style_) {
    PlatformShowWindow(browser);
  }
}

void CefBrowserClient::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::CloseAllBrowsers,
                                       this, force_close));
    return;
  }
  const BrowserList closing = browsers_;
  for (const CefRefPtr<CefBrowser>& browser : closing) {
    browser->GetHost()->CloseBrowser(force_close);
  }
}

void CefBrowserClient::HandleRelaunch(const std::string& url) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&CefBrowserClient::HandleRelaunch, this, url));
    return;
  }
  ShowMainWindow();
  if (url.empty() || browsers_.empty()) {
    return;
  }
  browsers_.front()->GetMainFrame()->LoadURL(url);
}

void CefBrowserClient::OnSmokeTimeout() {
  CEF_REQUIRE_UI_THREAD();
  if (!smoke_test_ || smoke_finished_) {
    return;
  }
  std::fprintf(stderr, "vrowser-smoke: timed out\n");
  FinishSmoke(2);
}

void CefBrowserClient::FinishSmoke(int code) {
  if (!smoke_test_ || smoke_finished_) {
    return;
  }
  smoke_finished_ = true;
  exit_code_ = code;
  CloseAllBrowsers(true);
}

}  // namespace vrowser
