// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#include <cstdio>
#include <optional>
#include <sstream>
#include <utility>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "browser/cef/views_delegates.hpp"

namespace vrowser {
namespace {

CefBrowserClient* g_instance = nullptr;

std::string DataUri(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

CefBrowserClient::CefBrowserClient(bool alloy_style,
                                   bool smoke_test,
                                   bool tab_smoke_test)
    : alloy_style_(alloy_style),
      smoke_test_(smoke_test),
      tab_smoke_test_(tab_smoke_test),
      exit_code_(smoke_test ? 2 : 0) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
  // The controller is a member, so the listener cannot outlive `this`.
  navigation_.SetListener(
      [this](const std::string& url) { LoadInMainFrame(url); });
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
  if (!browser->IsPopup()) {
    tabs_.Append(browser->GetIdentifier());
    ShowActiveTab();
  }

  if (smoke_test_ && !smoke_timer_armed_) {
    smoke_timer_armed_ = true;
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&CefBrowserClient::OnSmokeTimeout, this), 45000);
  }
}

bool CefBrowserClient::DoClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  const TabId id = browser->GetIdentifier();
  if (tabs_.Contains(id) && tabs_.size() > 1) {
    CefRefPtr<CefBrowserView> view = CefBrowserView::GetForBrowser(browser);
    CefRefPtr<CefWindow> window = view ? view->GetWindow() : nullptr;
    if (window) {
      // Other tabs remain, so close only this tab: take its view out of the
      // window instead of letting CEF close the window. Removing the last
      // reference to the view destroys the browser and calls OnBeforeClose.
      // Do it in a task so the view is not destroyed inside DoClose.
      tabs_.Close(id);
      ShowActiveTab();
      CefPostTask(TID_UI, base::BindOnce(
                              [](CefRefPtr<CefWindow> parent,
                                 CefRefPtr<CefBrowserView> child) {
                                parent->RemoveChildView(child);
                              },
                              window, view));
      return true;
    }
  }
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
  const TabId id = browser->GetIdentifier();
  if (tabs_.Close(id)) {
    ShowActiveTab();
  }
  TabSmokeClosed(id);
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
  if (tab_smoke_test_ && http_status_code >= 200 && http_status_code < 400) {
    TabSmokeLoadEnd(browser);
    return;
  }
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
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return;
  }
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
  if (!url.empty()) {
    Navigate(url);
  }
}

void CefBrowserClient::Navigate(const std::string& url) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::Navigate, this, url));
    return;
  }
  if (!navigation_.Request(url)) {
    std::fprintf(stderr, "vrowser: navigation rejected\n");
  }
}

void CefBrowserClient::LoadInMainFrame(const std::string& url) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  if (!browser) {
    return;
  }
  std::fprintf(stderr, "vrowser: navigate %s\n", url.c_str());
  browser->GetMainFrame()->LoadURL(url);
}

void CefBrowserClient::OpenTab(const std::string& url) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::OpenTab, this, url));
    return;
  }
  if (!alloy_style_) {
    // Observed on macOS with CEF 156: a second Chrome-style browser view added
    // to the window is never created. Tabs follow the Alloy shell plan in
    // docs/architecture/runtime-style.md.
    std::fprintf(stderr, "vrowser: tabs need --use-alloy-style\n");
    if (tab_smoke_test_) {
      FinishSmoke(1);
    }
    return;
  }
  CefRefPtr<CefBrowser> current = ActiveBrowser();
  CefRefPtr<CefBrowserView> current_view =
      current ? CefBrowserView::GetForBrowser(current) : nullptr;
  CefRefPtr<CefWindow> window =
      current_view ? current_view->GetWindow() : nullptr;
  if (!window) {
    std::fprintf(stderr, "vrowser: tabs need a Views window\n");
    return;
  }
  const cef_runtime_style_t runtime_style =
      alloy_style_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;
  CefBrowserSettings browser_settings;
  std::fprintf(stderr, "vrowser: open-tab %s\n", url.c_str());
  // OnAfterCreated appends the new browser to tabs_ and shows it.
  window->AddChildView(CefBrowserView::CreateBrowserView(
      this, url, browser_settings, nullptr, nullptr,
      CreateBrowserViewDelegate(runtime_style)));
}

void CefBrowserClient::CloseTab(TabId id) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::CloseTab, this, id));
    return;
  }
  if (CefRefPtr<CefBrowser> browser = FindBrowser(id)) {
    std::fprintf(stderr, "vrowser: close-tab %d\n", id);
    browser->GetHost()->CloseBrowser(false);
  }
}

bool CefBrowserClient::CanCloseWindow(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  bool ready = true;
  const BrowserList snapshot = browsers_;
  for (const CefRefPtr<CefBrowser>& browser : snapshot) {
    CefRefPtr<CefBrowserView> view = CefBrowserView::GetForBrowser(browser);
    CefRefPtr<CefWindow> parent = view ? view->GetWindow() : nullptr;
    if (!parent || !parent->IsSame(window)) {
      continue;
    }
    // A close that has already started must be allowed to finish. Returning
    // false then would leave the browser half-closed and the loop running.
    CefRefPtr<CefBrowserHost> host = browser->GetHost();
    if (!host->IsReadyToBeClosed() && !host->TryCloseBrowser()) {
      ready = false;
    }
  }
  return ready;
}

CefRefPtr<CefBrowser> CefBrowserClient::FindBrowser(TabId id) const {
  for (const CefRefPtr<CefBrowser>& browser : browsers_) {
    if (browser->GetIdentifier() == id) {
      return browser;
    }
  }
  return nullptr;
}

CefRefPtr<CefBrowser> CefBrowserClient::ActiveBrowser() const {
  if (const std::optional<TabId> active = tabs_.active()) {
    if (CefRefPtr<CefBrowser> browser = FindBrowser(*active)) {
      return browser;
    }
  }
  return browsers_.empty() ? nullptr : browsers_.front();
}

void CefBrowserClient::ShowActiveTab() {
  CEF_REQUIRE_UI_THREAD();
  const std::optional<TabId> active = tabs_.active();
  CefRefPtr<CefWindow> window;
  for (const TabId id : tabs_.tabs()) {
    CefRefPtr<CefBrowser> browser = FindBrowser(id);
    CefRefPtr<CefBrowserView> view =
        browser ? CefBrowserView::GetForBrowser(browser) : nullptr;
    if (!view) {
      continue;
    }
    view->SetVisible(active == id);
    if (!window) {
      window = view->GetWindow();
    }
  }
  if (window) {
    window->Layout();
  }
}

void CefBrowserClient::TabSmokeLoadEnd(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  const TabId id = browser->GetIdentifier();
  if (tab_smoke_stage_ == 0) {
    tab_smoke_stage_ = 1;
    tab_smoke_first_ = id;
    OpenTab(browser->GetMainFrame()->GetURL().ToString());
    return;
  }
  if (tab_smoke_stage_ == 1 && id != tab_smoke_first_) {
    tab_smoke_stage_ = 2;
    std::fprintf(stderr, "vrowser-smoke: tabs open %zu\n", tabs_.size());
    if (tabs_.size() != 2 || tabs_.active() != id) {
      FinishSmoke(1);
      return;
    }
    CloseTab(tab_smoke_first_);
  }
}

void CefBrowserClient::TabSmokeClosed(TabId id) {
  if (!tab_smoke_test_ || tab_smoke_stage_ != 2 || id != tab_smoke_first_) {
    return;
  }
  CefRefPtr<CefBrowser> remaining = ActiveBrowser();
  CefRefPtr<CefBrowserView> view =
      remaining ? CefBrowserView::GetForBrowser(remaining) : nullptr;
  const bool ok = tabs_.size() == 1 && view && view->IsVisible() &&
                  view->GetWindow() && view->GetWindow()->IsVisible();
  std::fprintf(stderr, "vrowser-smoke: tabs %s, %zu open, remaining %s\n",
               ok ? "ok" : "failed", tabs_.size(),
               view && view->IsVisible() ? "visible" : "hidden");
  FinishSmoke(ok ? 0 : 1);
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
