// Copyright 2026 The Vrowser Authors
// SPDX-License-Identifier: Apache-2.0

#include "browser/cef/cef_browser_client.hpp"

#include <cstdio>
#include <iterator>
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
#include "browser/cef/cef_renderer_app.hpp"
#include "browser/cef/views_delegates.hpp"
#include "vrowser/vim/key_event.hpp"

namespace vrowser {
namespace {

CefBrowserClient* g_instance = nullptr;

RawKeyType ToRawKeyType(cef_key_event_type_t type) {
  switch (type) {
    case KEYEVENT_RAWKEYDOWN:
      return RawKeyType::kRawKeyDown;
    case KEYEVENT_KEYDOWN:
      return RawKeyType::kKeyDown;
    case KEYEVENT_KEYUP:
      return RawKeyType::kKeyUp;
    case KEYEVENT_CHAR:
    default:
      return RawKeyType::kChar;
  }
}

RawKeyInput ToRawKeyInput(const CefKeyEvent& event) {
  RawKeyInput input;
  input.type = ToRawKeyType(event.type);
  input.windows_key_code = event.windows_key_code;
  // A lone UTF-16 surrogate is not a character on its own.
  const char16_t character = event.character;
  input.character = (character >= 0xD800 && character <= 0xDFFF)
                        ? 0
                        : static_cast<char32_t>(character);
  input.modifiers = event.modifiers;
  return input;
}

static_assert(kRawModifierShift == EVENTFLAG_SHIFT_DOWN);
static_assert(kRawModifierControl == EVENTFLAG_CONTROL_DOWN);
static_assert(kRawModifierAlt == EVENTFLAG_ALT_DOWN);
static_assert(kRawModifierMeta == EVENTFLAG_COMMAND_DOWN);
static_assert(kRawModifierRepeat == EVENTFLAG_IS_REPEAT);

std::string DataUri(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

CefBrowserClient::CefBrowserClient(const StartupConfig& config)
    : alloy_style_(config.alloy_style),
      smoke_test_(config.smoke_test),
      tab_smoke_test_(config.tab_smoke_test),
      key_smoke_test_(config.key_smoke_test),
      exit_code_(config.smoke_test ? 2 : 0) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
  // The controller is a member, so the listener cannot outlive `this`.
  navigation_.SetListener(
      [this](const std::string& url) { LoadInMainFrame(url); });
  RegisterCommands();
}

void CefBrowserClient::RegisterCommands() {
  // `this` owns the registry, so handlers cannot outlive it. Handlers run on
  // the UI thread from OnPreKeyEvent.
  commands_.Register("reload", "Reload the current page",
                     [this](const std::vector<std::string>&) {
                       if (CefRefPtr<CefBrowser> browser = ActiveBrowser()) {
                         browser->Reload();
                       }
                       return CommandResult::Ok();
                     });
  bindings_.Bind(Mode::kNormal, "r", "reload");
}

void CefBrowserClient::RunBinding(const KeyDispatch& dispatch) {
  const CommandResult result =
      commands_.Run(dispatch.action->command, dispatch.action->args,
                    dispatch.count, dispatch.has_count);
  std::fprintf(
      stderr, "vrowser: command %s%s%s\n", dispatch.action->command.c_str(),
      result.ok() ? "" : " failed: ", std::string(result.error()).c_str());
  if (dispatch.has_count && dispatch.count != 1) {
    std::fprintf(stderr, "vrowser: count %d\n", dispatch.count);
  }
}

bool CefBrowserClient::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();
  // Renderer messages are untrusted. Accept only the one message the
  // renderer app sends, with the expected argument type.
  if (source_process != PID_RENDERER ||
      message->GetName() != kEditableFocusMessage) {
    return false;
  }
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  if (args->GetSize() != 1 || args->GetType(0) != VTYPE_BOOL) {
    return true;
  }
  const bool editable = args->GetBool(0);
  KeyInputState& input = key_inputs_[browser->GetIdentifier()];
  const Mode before = input.mode.mode();
  NoteEditableFocus(input, editable);
  if (input.mode.mode() != before) {
    std::fprintf(stderr, "vrowser: mode %s\n",
                 input.mode.mode() == Mode::kInsert ? "insert" : "normal");
    if (key_smoke_test_) {
      KeySmokeStep(browser, input.mode.mode() == Mode::kInsert ? "mode insert"
                                                               : "mode normal");
    }
  }
  return true;
}

bool CefBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                     const CefKeyEvent& event,
                                     CefEventHandle os_event,
                                     bool* is_keyboard_shortcut) {
  CEF_REQUIRE_UI_THREAD();
  const std::optional<KeyEvent> key = NormalizeKey(ToRawKeyInput(event));
  if (!key) {
    return false;
  }
  KeyInputState& input = key_inputs_[browser->GetIdentifier()];
  const Mode before = input.mode.mode();
  const KeyDispatch dispatch = DispatchKey(bindings_, input, *key);
  if (input.mode.mode() != before) {
    std::fprintf(stderr, "vrowser: mode %s\n",
                 input.mode.mode() == Mode::kInsert ? "insert" : "normal");
    if (key_smoke_test_) {
      KeySmokeStep(browser, input.mode.mode() == Mode::kInsert ? "mode insert"
                                                               : "mode normal");
    }
  }
  if (dispatch.action) {
    RunBinding(dispatch);
  }
  return dispatch.disposition == KeyDisposition::kConsume;
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
  if (key_smoke_test_) {
    KeySmokeStep(browser, "title " + title.ToString());
  }
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
  key_inputs_.erase(id);
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
  (void)transition_type;
  if (frame->IsMain()) {
    // The load can drop a key-up. Forget consumed presses so the next key-up
    // of that key is not swallowed.
    NoteNavigation(key_inputs_[browser->GetIdentifier()]);
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
  if (key_smoke_test_) {
    KeySmokeStep(browser, "load-end");
    return;
  }
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

namespace vrowser {

std::string CefBrowserClient::KeySmokePageUrl() {
  // Logs every keydown the page sees into the title, with the focused id.
  return DataUri(
      "<!doctype html><title>ready</title><input id=f>"
      "<script>addEventListener('keydown', e => { document.title = 'key:' + "
      "e.key + ':' + (document.activeElement.id || 'body'); }, true);"
      "</script>",
      "text/html");
}

void CefBrowserClient::SendTestKey(CefRefPtr<CefBrowser> browser,
                                   int windows_key_code,
                                   int mac_key_code,
                                   int linux_key_code,
                                   char16_t character) {
  CefRefPtr<CefBrowserHost> host = browser->GetHost();
  host->SetFocus(true);
#if defined(OS_MAC)
  // CefBrowserHost::SendKeyEvent trips a Chromium DCHECK for windowed
  // browsers on macOS, so send a native event instead.
  (void)windows_key_code;
  (void)linux_key_code;
  PlatformSendTestKey(browser, mac_key_code, character);
#else
  // Linux windowed CEF reports X11 keycodes (evdev code + 8), not macOS
  // virtual key codes. A focused field drops a key whose native code is
  // wrong even when the same event reaches the page body. Windows uses the
  // virtual key as the native code; this path has not been run there.
  (void)mac_key_code;
  CefKeyEvent event;
  event.windows_key_code = windows_key_code;
#if defined(OS_LINUX)
  event.native_key_code = linux_key_code;
#else
  event.native_key_code = windows_key_code;
  (void)linux_key_code;
#endif
  event.character = character;
  event.unmodified_character = character;
  event.type = KEYEVENT_RAWKEYDOWN;
  host->SendKeyEvent(event);
  if (character >= 0x20) {
    // Linux CHAR events carry the character in windows_key_code, not the
    // virtual key. Sending the virtual key here skips the DOM keydown once
    // an editor is focused.
    event.type = KEYEVENT_CHAR;
    event.windows_key_code = character;
    host->SendKeyEvent(event);
    event.windows_key_code = windows_key_code;
  }
  event.type = KEYEVENT_KEYUP;
  host->SendKeyEvent(event);
#endif
}

// Each step waits for one event, then acts. Any page title showing a key the
// shell should have consumed fails the run.
void CefBrowserClient::KeySmokeStep(CefRefPtr<CefBrowser> browser,
                                    const std::string& event) {
  CEF_REQUIRE_UI_THREAD();
  if (smoke_finished_) {
    return;
  }
  if (alloy_style_) {
    // Observed on macOS: synthesized key events reach OnPreKeyEvent but not
    // the page in Alloy style, with or without a keyboard handler. Real typing
    // has to be checked by hand there.
    std::fprintf(stderr, "vrowser-smoke: key smoke needs Chrome style\n");
    FinishSmoke(1);
    return;
  }
  if (event == "title key:r:body" || event.starts_with("title key:Escape")) {
    std::fprintf(stderr, "vrowser-smoke: keys failed, page saw %s\n",
                 event.c_str());
    FinishSmoke(1);
    return;
  }
  constexpr int kVkX = 'X', kMacX = 7, kLinuxX = 53;
  constexpr int kVkR = 'R', kMacR = 15, kLinuxR = 27;
  constexpr int kVkEscape = 0x1B, kMacEscape = 53, kLinuxEscape = 9;

  struct Step {
    const char* wait_for;
    const char* note;
  };
  static constexpr Step kSteps[] = {
      {"load-end", "page loaded; unbound x goes to the page"},
      {"title key:x:body", "bound r reloads and stays out of the page"},
      {"load-end", "reloaded; focus the text field"},
      {"mode insert", "Insert mode; r goes to the field"},
      {"title key:r:f", "Escape leaves Insert"},
      {"mode normal", "Normal mode; x goes to the page, Escape did not"},
      {"title key:x:f", "done"},
  };
  if (key_smoke_step_ >= static_cast<int>(std::size(kSteps)) ||
      event != kSteps[key_smoke_step_].wait_for) {
    return;
  }
  std::fprintf(stderr, "vrowser-smoke: step %d %s: %s\n", key_smoke_step_,
               event.c_str(), kSteps[key_smoke_step_].note);
  switch (key_smoke_step_++) {
    case 0:
      CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                         browser, kVkX, kMacX, kLinuxX, u'x'));
      break;
    case 1:
      CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                         browser, kVkR, kMacR, kLinuxR, u'r'));
      break;
    case 2:
      browser->GetMainFrame()->ExecuteJavaScript(
          "document.getElementById('f').focus()", "", 0);
      break;
    case 3:
      // OnFocusedNodeChanged can arrive before the editor is ready to take a
      // key. Sending immediately is flaky on Linux: the event is forwarded
      // but never becomes a DOM keydown. A short delay lets focus settle.
      CefPostDelayedTask(TID_UI,
                         base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                        browser, kVkR, kMacR, kLinuxR, u'r'),
                         200);
      break;
    case 4:
      CefPostTask(TID_UI,
                  base::BindOnce(&CefBrowserClient::SendTestKey, this, browser,
                                 kVkEscape, kMacEscape, kLinuxEscape, 0x1B));
      break;
    case 5:
      CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                         browser, kVkX, kMacX, kLinuxX, u'x'));
      break;
    case 6:
      std::fprintf(stderr, "vrowser-smoke: keys ok\n");
      FinishSmoke(0);
      break;
  }
}

}  // namespace vrowser
