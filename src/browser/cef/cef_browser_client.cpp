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
#include "vrowser/ui/status_line.hpp"
#include "vrowser/vim/default_bindings.hpp"
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

// Smoke-test keys: Windows virtual key, macOS key code, X11 keycode,
// character.
constexpr TestKey kTestQ{'Q', 12, 24, u'q'};
constexpr TestKey kTestR{'R', 15, 27, u'r'};
constexpr TestKey kTestEscape{0x1B, 53, 9, 0x1B};
constexpr TestKey kTestJ{'J', 38, 44, u'j'};
constexpr TestKey kTestK{'K', 40, 45, u'k'};
constexpr TestKey kTestD{'D', 2, 40, u'd'};
constexpr TestKey kTestU{'U', 32, 30, u'u'};
constexpr TestKey kTestG{'G', 5, 42, u'g'};
constexpr TestKey kTestShiftG{'G', 5, 42, u'G', true};
constexpr TestKey kTestT{'T', 17, 28, u't'};
constexpr TestKey kTestShiftT{'T', 17, 28, u'T', true};
constexpr TestKey kTestX{'X', 7, 53, u'x'};
constexpr TestKey kTest3{'3', 20, 12, u'3'};
constexpr TestKey kTestShiftH{'H', 4, 43, u'H', true};
constexpr TestKey kTestColon{0xBA, 41, 47, u':', true, u';'};
constexpr TestKey kTestEnter{0x0D, 36, 36, 0x0D};
constexpr TestKey kTestShiftL{'L', 37, 46, u'L', true};

struct ScrollCommand {
  const char* name;
  const char* description;
  ScrollMotion motion;
};
constexpr ScrollCommand kScrollCommands[] = {
    {"scroll-down", "Scroll down one line step", ScrollMotion::kLineDown},
    {"scroll-up", "Scroll up one line step", ScrollMotion::kLineUp},
    {"scroll-half-page-down", "Scroll down half a page",
     ScrollMotion::kHalfPageDown},
    {"scroll-half-page-up", "Scroll up half a page", ScrollMotion::kHalfPageUp},
    {"scroll-to-top", "Scroll to the top of the page", ScrollMotion::kTop},
    {"scroll-to-bottom", "Scroll to the bottom of the page",
     ScrollMotion::kBottom},
};
std::optional<ScrollMotion> MotionForCommand(const std::string& command) {
  for (const ScrollCommand& entry : kScrollCommands) {
    if (command == entry.name) {
      return entry.motion;
    }
  }
  return std::nullopt;
}

// Keep-alives are throttled; OS repeat can be far faster than needed.
constexpr auto kHeldKeepAliveInterval = std::chrono::milliseconds(150);

std::string DataUri(const std::string& data, const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

}  // namespace

CefBrowserClient::CefBrowserClient(const StartupConfig& config)
    : alloy_style_(config.alloy_style),
      new_tab_url_(config.initial_url),
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
  PlatformWatchKeyUps(true);
}

void CefBrowserClient::RegisterCommands() {
  // `this` owns the registry, so handlers cannot outlive it. Handlers run on
  // the UI thread from OnPreKeyEvent.
  commands_.Register("reload", "Reload the current page",
                     [this](const std::vector<std::string>&) {
                       if (CefRefPtr<CefBrowser> browser = CommandBrowser()) {
                         browser->Reload();
                       }
                       return CommandResult::Ok();
                     });
  for (const ScrollCommand& command : kScrollCommands) {
    const ScrollMotion motion = command.motion;
    commands_.RegisterCounted(
        command.name, command.description,
        [this, motion](const std::vector<std::string>&, int count) {
          ScrollFocusedFrame(motion, count);
          return CommandResult::Ok();
        });
  }
  commands_.RegisterCounted("tab-next",
                            "Show the next tab, wrapping to the first",
                            [this](const std::vector<std::string>&, int count) {
                              SwitchTab(count);
                              return CommandResult::Ok();
                            });
  commands_.RegisterCounted("tab-previous",
                            "Show the previous tab, wrapping to the last",
                            [this](const std::vector<std::string>&, int count) {
                              SwitchTab(-count);
                              return CommandResult::Ok();
                            });
  // Counted so that a count does not queue several navigations; one step per
  // key until a multi-step history API is chosen.
  commands_.RegisterCounted("history-back",
                            "Go back one page in the current tab",
                            [this](const std::vector<std::string>&, int) {
                              CefRefPtr<CefBrowser> browser = CommandBrowser();
                              if (browser && browser->CanGoBack()) {
                                browser->GoBack();
                              }
                              return CommandResult::Ok();
                            });
  commands_.RegisterCounted("history-forward",
                            "Go forward one page in the current tab",
                            [this](const std::vector<std::string>&, int) {
                              CefRefPtr<CefBrowser> browser = CommandBrowser();
                              if (browser && browser->CanGoForward()) {
                                browser->GoForward();
                              }
                              return CommandResult::Ok();
                            });
  // Counted so that `3t` opens one tab, like `3x` below.
  commands_.RegisterCounted("tab-new", "Open a new tab with the start page",
                            [this](const std::vector<std::string>&, int) {
                              OpenTab(new_tab_url_);
                              return CommandResult::Ok();
                            });
  // Counted so that `3x` closes one tab instead of asking the same browser to
  // close three times.
  commands_.RegisterCounted(
      "tab-close", "Close the current tab; closing the last one quits",
      [this](const std::vector<std::string>&, int) {
        if (CefRefPtr<CefBrowser> browser = CommandBrowser()) {
          CloseTab(browser->GetIdentifier());
        }
        return CommandResult::Ok();
      });
  commands_.Register("command-line", "Open the command line",
                     [this](const std::vector<std::string>&) {
                       OpenCommandLine();
                       return CommandResult::Ok();
                     });
  commands_.Register("open", "Load a URL in the current tab",
                     [this](const std::vector<std::string>& args) {
                       if (args.size() != 1) {
                         return CommandResult::Error("Usage: open <url>");
                       }
                       // NavigationController rejects empty and javascript:
                       // URLs.
                       if (!navigation_.Request(args[0])) {
                         return CommandResult::Error("Cannot open: " + args[0]);
                       }
                       return CommandResult::Ok();
                     });
  CHECK(AddDefaultBindings(bindings_));
}

void CefBrowserClient::SwitchTab(int steps) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> from = CommandBrowser();
  // A popup has its own window and is not a tab.
  if (!from || !tabs_.Contains(from->GetIdentifier()) || steps == 0) {
    return;
  }
  const std::optional<TabId> before = tabs_.active();
  const std::optional<TabId> after =
      steps > 0 ? tabs_.ActivateNext(steps) : tabs_.ActivatePrevious(-steps);
  if (!before || !after || *after == *before) {
    return;
  }
  StopHeldScroll();
  // The key that switched is still down; its key-up reaches the new tab.
  TransferHeldKeys(key_inputs_[*before], key_inputs_[*after]);
  ShowActiveTab();
  if (CefRefPtr<CefBrowser> browser = FindBrowser(*after)) {
    browser->GetHost()->SetFocus(true);
  }
  ShowActiveTitle();
  UpdateStatusBar();
  std::fprintf(stderr, "vrowser: tab %d active\n", *after);
  TabSmokeActivated(*after);
}

void CefBrowserClient::AttachCommandLine(CefRefPtr<CefTextfield> command_line) {
  CEF_REQUIRE_UI_THREAD();
  command_line_ = command_line;
}

void CefBrowserClient::OpenCommandLine() {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = CommandBrowser();
  if (!command_line_ || !command_line_->IsValid()) {
    std::fprintf(stderr, "vrowser: the command line needs --use-alloy-style\n");
    return;
  }
  if (!browser || command_line_open_) {
    return;
  }
  command_tab_ = browser->GetIdentifier();
  KeyInputState& input = key_inputs_[command_tab_];
  input.mode.Apply(ModeEvent::kEnterCommand);
  input.sequence = KeySequenceState{};
  command_line_open_ = true;
  // CefTextfield::SetText rejects an empty string; select and delete instead.
  if (!command_line_->GetText().empty()) {
    command_line_->SelectAll(false);
    command_line_->ExecuteCommand(CEF_TFC_DELETE);
  }
  command_line_->SetVisible(true);
  if (CefRefPtr<CefWindow> window = command_line_->GetWindow()) {
    window->Layout();
  }
  // Focus in a task: the `:` key event is still being delivered to the page's
  // view, and its character must not land in the command line.
  CefPostTask(TID_UI, base::BindOnce(
                          [](CefRefPtr<CefTextfield> line) {
                            if (line->IsValid() && line->IsVisible()) {
                              line->RequestFocus();
                            }
                          },
                          command_line_));
  std::fprintf(stderr, "vrowser: mode command\n");
  UpdateStatusBar();
}

void CefBrowserClient::CloseCommandLine() {
  CEF_REQUIRE_UI_THREAD();
  if (!command_line_open_) {
    return;
  }
  // Cleared first: focusing the page blurs the line, which calls this again.
  command_line_open_ = false;
  if (command_line_ && command_line_->IsValid()) {
    command_line_->SetVisible(false);
    if (CefRefPtr<CefWindow> window = command_line_->GetWindow()) {
      window->Layout();
    }
  }
  const auto it = key_inputs_.find(command_tab_);
  if (it != key_inputs_.end() && it->second.mode.mode() == Mode::kCommand) {
    it->second.mode.Apply(ModeEvent::kEscape);
  }
  if (CefRefPtr<CefBrowser> browser = FindBrowser(command_tab_)) {
    browser->GetHost()->SetFocus(true);
  }
  std::fprintf(stderr, "vrowser: mode normal\n");
  UpdateStatusBar();
}

void CefBrowserClient::SubmitCommandLine(const std::string& text) {
  CEF_REQUIRE_UI_THREAD();
  CloseCommandLine();
  const ParsedCommand parsed = ParseCommandLine(text);
  if (parsed.name.empty()) {
    return;
  }
  const CommandResult result = commands_.Execute(text);
  std::fprintf(
      stderr, "vrowser: command-line %s%s%s\n", parsed.name.c_str(),
      result.ok() ? "" : " failed: ", std::string(result.error()).c_str());
  status_message_ = result.ok() ? std::string() : std::string(result.error());
  UpdateStatusBar();
}

void CefBrowserClient::AttachStatusBar(CefRefPtr<CefTextfield> status_bar) {
  CEF_REQUIRE_UI_THREAD();
  status_bar_ = status_bar;
  UpdateStatusBar();
}

void CefBrowserClient::UpdateStatusBar() {
  CEF_REQUIRE_UI_THREAD();
  if (!status_bar_ || !status_bar_->IsValid()) {
    return;
  }
  StatusLineInput input;
  if (CefRefPtr<CefBrowser> browser = ActiveBrowser()) {
    const TabId id = browser->GetIdentifier();
    // The committed URL as the browser process knows it. Never the title or
    // anything the page reports about itself.
    input.url = browser->GetMainFrame()->GetURL().ToString();
    input.loading = loading_tabs_.count(id) > 0;
    const auto it = key_inputs_.find(id);
    if (it != key_inputs_.end()) {
      input.mode = it->second.mode.mode();
      input.pending = it->second.sequence;
    }
  }
  input.message = status_message_;
  // About 7 px per character of the 12 px monospace font.
  const int width = status_bar_->GetBounds().width;
  const size_t max_chars = width > 0 ? static_cast<size_t>(width) / 7 : 160;
  status_bar_->SetText(FormatStatusLine(input, max_chars));
}

std::string CefBrowserClient::StatusBarText() const {
  return status_bar_ ? status_bar_->GetText().ToString() : std::string();
}

void CefBrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& url) {
  CEF_REQUIRE_UI_THREAD();
  (void)browser;
  (void)url;
  if (frame->IsMain()) {
    UpdateStatusBar();
  }
}

void CefBrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                            bool is_loading,
                                            bool can_go_back,
                                            bool can_go_forward) {
  CEF_REQUIRE_UI_THREAD();
  (void)can_go_back;
  (void)can_go_forward;
  if (is_loading) {
    loading_tabs_.insert(browser->GetIdentifier());
  } else {
    loading_tabs_.erase(browser->GetIdentifier());
  }
  UpdateStatusBar();
}

void CefBrowserClient::ShowActiveTitle() {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  CefRefPtr<CefBrowserView> view =
      browser ? CefBrowserView::GetForBrowser(browser) : nullptr;
  CefRefPtr<CefWindow> window = view ? view->GetWindow() : nullptr;
  if (!window) {
    return;
  }
  CefRefPtr<CefNavigationEntry> entry =
      browser->GetHost()->GetVisibleNavigationEntry();
  if (!entry) {
    return;
  }
  const CefString title = entry->GetTitle();
  window->SetTitle(title.empty() ? entry->GetURL() : title);
}

void CefBrowserClient::ScrollFocusedFrame(ScrollMotion motion, int count) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefBrowser> browser = CommandBrowser();
  if (!browser) {
    return;
  }
  CefRefPtr<CefFrame> frame = browser->GetFocusedFrame();
  if (!frame) {
    frame = browser->GetMainFrame();
  }
  // Runs in the page's main world. The script holds only constants and the
  // count, so no page or user text is evaluated. A page that redefines
  // window.scrollBy only breaks scrolling for itself.
  frame->ExecuteJavaScript(ScrollScript(motion, count), "", 0);
}

void CefBrowserClient::OnNativeKeyUp(int native_key_code) {
  CEF_REQUIRE_UI_THREAD();
  if (held_scroll_ && held_scroll_->native_key_code == native_key_code) {
    StopHeldScroll();
  }
}

void CefBrowserClient::HandleHold(CefRefPtr<CefBrowser> browser,
                                  const KeyDispatch& dispatch,
                                  int native_key_code) {
  CEF_REQUIRE_UI_THREAD();
  if (dispatch.hold == HoldEvent::kNone || !dispatch.held_action) {
    return;
  }
  if (dispatch.hold == HoldEvent::kStop) {
    StopHeldScroll();
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  if (dispatch.hold == HoldEvent::kContinue) {
    if (held_scroll_ && held_scroll_->tab == browser->GetIdentifier() &&
        now - held_scroll_->last_keep_alive >= kHeldKeepAliveInterval &&
        held_scroll_->frame->IsValid()) {
      held_scroll_->frame->ExecuteJavaScript(HeldScrollKeepAliveScript(), "",
                                             0);
      held_scroll_->last_keep_alive = now;
    }
    return;
  }
  // kStart. Only line and half-page scrolls keep going; `r`, `gg`, `G`, and
  // the tab keys act once.
  const std::optional<ScrollMotion> motion =
      MotionForCommand(dispatch.held_action->command);
  if (!motion || !IsContinuousMotion(*motion)) {
    return;
  }
  StopHeldScroll();
  CefRefPtr<CefFrame> frame = browser->GetFocusedFrame();
  if (!frame) {
    frame = browser->GetMainFrame();
  }
  frame->ExecuteJavaScript(HeldScrollStartScript(*motion), "", 0);
  held_scroll_ =
      HeldScroll{browser->GetIdentifier(), frame, native_key_code, now};
  std::fprintf(stderr, "vrowser: hold %s\n",
               dispatch.held_action->command.c_str());
}

void CefBrowserClient::StopHeldScroll() {
  CEF_REQUIRE_UI_THREAD();
  if (!held_scroll_) {
    return;
  }
  if (held_scroll_->frame->IsValid()) {
    held_scroll_->frame->ExecuteJavaScript(HeldScrollStopScript(), "", 0);
  }
  held_scroll_.reset();
  std::fprintf(stderr, "vrowser: hold end\n");
}

void CefBrowserClient::RunBinding(CefRefPtr<CefBrowser> browser,
                                  const KeyDispatch& dispatch) {
  key_target_ = browser;
  const CommandResult result =
      commands_.Run(dispatch.action->command, dispatch.action->args,
                    dispatch.count, dispatch.has_count);
  key_target_ = nullptr;
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
  if (editable && held_scroll_ &&
      held_scroll_->tab == browser->GetIdentifier()) {
    StopHeldScroll();
  }
  KeyInputState& input = key_inputs_[browser->GetIdentifier()];
  const Mode before = input.mode.mode();
  NoteEditableFocus(input, editable);
  UpdateStatusBar();
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
  if (key->key_down && !key->repeat && !key->character_only) {
    status_message_.clear();
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
    RunBinding(browser, dispatch);
  }
  HandleHold(browser, dispatch, event.native_key_code);
  UpdateStatusBar();
  return dispatch.disposition == KeyDisposition::kConsume;
}

CefBrowserClient::~CefBrowserClient() {
  PlatformWatchKeyUps(false);
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
  const TabId id = browser->GetIdentifier();
  if (tabs_.Contains(id) && tabs_.active() != id) {
    // A background tab does not own the window title.
    return;
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
    const std::optional<TabId> previous = tabs_.active();
    const TabId id = browser->GetIdentifier();
    tabs_.Append(id);
    ShowActiveTab();
    if (previous) {
      // A new tab next to others (`t`): it takes focus, and the key that
      // opened it, if still down, has its key-up consumed here.
      TransferHeldKeys(key_inputs_[*previous], key_inputs_[id]);
      browser->GetHost()->SetFocus(true);
    }
    UpdateStatusBar();
  }

  if (smoke_test_ && !smoke_timer_armed_) {
    smoke_timer_armed_ = true;
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&CefBrowserClient::OnSmokeTimeout, this), 45000);
  }
}

void CefBrowserClient::RemoveTabView(CefRefPtr<CefPanel> tabs,
                                     CefRefPtr<CefBrowserView> view) {
  CEF_REQUIRE_UI_THREAD();
  tabs->RemoveChildView(view);
  // The removed view held keyboard focus, and Views does not hand it to the
  // tab that is now shown.
  if (CefRefPtr<CefBrowser> active = ActiveBrowser()) {
    active->GetHost()->SetFocus(true);
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
      if (CefRefPtr<CefBrowser> active = ActiveBrowser()) {
        // A closing key (`x`) is still down; its key-up reaches this tab.
        TransferHeldKeys(key_inputs_[id], key_inputs_[active->GetIdentifier()]);
        active->GetHost()->SetFocus(true);
      }
      ShowActiveTitle();
      UpdateStatusBar();
      CefRefPtr<CefView> parent = view->GetParentView();
      CefRefPtr<CefPanel> panel = parent ? parent->AsPanel() : nullptr;
      if (panel) {
        CefPostTask(TID_UI, base::BindOnce(&CefBrowserClient::RemoveTabView,
                                           this, panel, view));
      }
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
  if (held_scroll_ && held_scroll_->tab == id) {
    held_scroll_.reset();
  }
  loading_tabs_.erase(id);
  if (tabs_.Close(id)) {
    ShowActiveTab();
  }
  TabSmokeClosed(id);
  if (browsers_.empty()) {
    // The window is gone. CEF requires every reference to be released before
    // CefShutdown.
    status_bar_ = nullptr;
    command_line_ = nullptr;
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
    if (held_scroll_ && held_scroll_->tab == browser->GetIdentifier()) {
      StopHeldScroll();
    }
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
  // Tab views share one parent panel; see CreateWindowDelegate.
  CefRefPtr<CefView> parent =
      current_view ? current_view->GetParentView() : nullptr;
  CefRefPtr<CefPanel> tabs = parent ? parent->AsPanel() : nullptr;
  if (!tabs) {
    std::fprintf(stderr, "vrowser: tabs need a Views window\n");
    return;
  }
  const cef_runtime_style_t runtime_style =
      alloy_style_ ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_DEFAULT;
  CefBrowserSettings browser_settings;
  std::fprintf(stderr, "vrowser: open-tab %s\n", url.c_str());
  // OnAfterCreated appends the new browser to tabs_ and shows it.
  tabs->AddChildView(CefBrowserView::CreateBrowserView(
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

CefRefPtr<CefBrowser> CefBrowserClient::CommandBrowser() const {
  return key_target_ ? key_target_ : ActiveBrowser();
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

// Tab smoke stages:
//   0  wait for the first tab's load, then type `t` to open a second tab.
//   1  wait for the second tab's load, then type `gt` in it.
//   2  `gt` wrapped to the first tab; type `gT`.
//   3  `gT` wrapped to the second tab; type `x`.
//   4  `x` closed the second tab; check the first is shown.
//   5  macOS: the command line steps (CommandSmokeStep) end with
//      `:open https://example.org/`. Elsewhere go straight to 7.
//   6  example.org loaded from `:open`; check the status bar.
//   7  `x` closed the last tab; the process quits with 0.
void CefBrowserClient::TabSmokeLoadEnd(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  const TabId id = browser->GetIdentifier();
  if (tab_smoke_stage_ == 0) {
    tab_smoke_stage_ = 1;
    tab_smoke_first_ = id;
    if (!TabSmokeStatusBarShows(browser)) {
      FinishSmoke(1);
      return;
    }
    SendTestKeys(browser, {kTestT});
    return;
  }
  if (tab_smoke_stage_ == 6 &&
      browser->GetMainFrame()->GetURL().ToString().find("example.org") !=
          std::string::npos) {
    if (!TabSmokeStatusBarShows(browser)) {
      FinishSmoke(1);
      return;
    }
    TabSmokeCloseLast();
    return;
  }
  if (tab_smoke_stage_ == 1 && id != tab_smoke_first_) {
    tab_smoke_stage_ = 2;
    tab_smoke_second_ = id;
    std::fprintf(stderr, "vrowser-smoke: tabs open %zu\n", tabs_.size());
    if (tabs_.size() != 2 || tabs_.active() != id ||
        !TabSmokeStatusBarShows(browser)) {
      FinishSmoke(1);
      return;
    }
    SendTestKeys(browser, {kTestG, kTestT});
  }
}

bool CefBrowserClient::TabSmokeStatusBarShows(CefRefPtr<CefBrowser> browser) {
  // The bar must show Normal mode and the URL the browser committed.
  const std::string text = StatusBarText();
  const std::string url = browser->GetMainFrame()->GetURL().ToString();
  const bool ok =
      text.starts_with("NORMAL  ") && text.find(url) != std::string::npos;
  std::fprintf(stderr, "vrowser-smoke: status bar %s: \"%s\"\n",
               ok ? "ok" : "failed", text.c_str());
  return ok;
}

void CefBrowserClient::TabSmokeActivated(TabId id) {
  if (!tab_smoke_test_ || smoke_finished_) {
    return;
  }
  const bool expected = (tab_smoke_stage_ == 2 && id == tab_smoke_first_) ||
                        (tab_smoke_stage_ == 3 && id == tab_smoke_second_);
  std::fprintf(stderr, "vrowser-smoke: %s activated tab %d\n",
               tab_smoke_stage_ == 2 ? "gt" : "gT", id);
  if (!expected) {
    FinishSmoke(1);
    return;
  }
  const bool to_first = tab_smoke_stage_ == 2;
  ++tab_smoke_stage_;
  if (CefRefPtr<CefBrowser> browser = FindBrowser(id)) {
    if (to_first) {
      SendTestKeys(browser, {kTestG, kTestShiftT});
    } else {
      SendTestKeys(browser, {kTestX});
    }
  }
}

void CefBrowserClient::TabSmokeClosed(TabId id) {
  if (!tab_smoke_test_ || smoke_finished_) {
    return;
  }
  if (tab_smoke_stage_ == 7 && id == tab_smoke_first_) {
    std::fprintf(stderr, "vrowser-smoke: x closed the last tab, quitting\n");
    return;
  }
  if (tab_smoke_stage_ != 4 || id != tab_smoke_second_) {
    return;
  }
  CefRefPtr<CefBrowser> remaining = ActiveBrowser();
  CefRefPtr<CefBrowserView> view =
      remaining ? CefBrowserView::GetForBrowser(remaining) : nullptr;
  const bool ok = tabs_.size() == 1 && tabs_.active() == tab_smoke_first_ &&
                  view && view->IsVisible() && view->GetWindow() &&
                  view->GetWindow()->IsVisible();
  std::fprintf(stderr, "vrowser-smoke: tabs %s, %zu open, remaining %s\n",
               ok ? "ok" : "failed", tabs_.size(),
               view && view->IsVisible() ? "visible" : "hidden");
  if (!ok) {
    FinishSmoke(1);
    return;
  }
#if defined(OS_MAC)
  // Synthesized keys reach the Views command line only on macOS, where they
  // are native events sent to the window.
  // Wait for focus to return to the remaining tab after the closed view is
  // removed.
  tab_smoke_stage_ = 5;
  SendTestKeys(remaining, {kTestColon}, 300);
  CefPostDelayedTask(
      TID_UI, base::BindOnce(&CefBrowserClient::CommandSmokeStep, this, 0),
      700);
#else
  TabSmokeCloseLast();
#endif
}

void CefBrowserClient::TabSmokeCloseLast() {
  // Closing the last tab must quit on its own. The 45 s smoke timeout still
  // applies and exits 2 if it does not.
  tab_smoke_stage_ = 7;
  exit_code_ = 0;
  if (CefRefPtr<CefBrowser> browser = ActiveBrowser()) {
    SendTestKeys(browser, {kTestX});
  }
}

// Each step checks the state the previous keys should have left, then types
// the next keys and schedules the next step:
//   0  `:` opened the line in Command mode; submit `nope`.
//   1  the line closed and the bar reports the unknown command; type `:`.
//   2  the line is open; type `nope2` and Escape.
//   3  the line closed and nothing ran; type `:`.
//   4  the line is open; submit `open https://example.org/` (stage 6).
void CefBrowserClient::CommandSmokeStep(int step) {
  CEF_REQUIRE_UI_THREAD();
  if (smoke_finished_) {
    return;
  }
  CefRefPtr<CefBrowser> browser = ActiveBrowser();
  const std::string bar = StatusBarText();
  const bool normal = bar.starts_with("NORMAL  ");
  bool ok = false;
  const char* text = nullptr;
  TestKey key = kTestEnter;
  switch (step) {
    case 0:
      ok = CommandLineOpen() && bar.starts_with("COMMAND  ");
      text = "nope";
      break;
    case 1:
      ok = !CommandLineOpen() && normal &&
           bar.find("| Unknown command: nope") != std::string::npos;
      key = kTestColon;
      break;
    case 2:
      ok = CommandLineOpen();
      text = "nope2";
      key = kTestEscape;
      break;
    case 3:
      ok = !CommandLineOpen() && normal &&
           bar.find("nope2") == std::string::npos;
      key = kTestColon;
      break;
    case 4:
      ok = CommandLineOpen();
      text = "open https://example.org/";
      break;
  }
  std::fprintf(stderr, "vrowser-smoke: command step %d %s: \"%s\"\n", step,
               ok ? "ok" : "failed", bar.c_str());
  if (!ok || !browser) {
    FinishSmoke(1);
    return;
  }
  if (text != nullptr) {
    command_line_->SetText(text);
  }
  SendTestKeys(browser, {key});
  if (step == 4) {
    tab_smoke_stage_ = 6;
    return;
  }
  CefPostDelayedTask(
      TID_UI,
      base::BindOnce(&CefBrowserClient::CommandSmokeStep, this, step + 1), 400);
}

void CefBrowserClient::SendTestKeys(CefRefPtr<CefBrowser> browser,
                                    std::initializer_list<TestKey> keys,
                                    int64_t delay_ms) {
  for (const TestKey& key : keys) {
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                      browser, key, TestKeyPhase::kPress),
                       delay_ms);
  }
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
  // Logs every keydown the page sees into the title, with the focused id, and
  // every scroll as "scroll:<y>", "scroll:half" (half a viewport down), or
  // "scroll:bottom". The spacer makes the page taller than the window.
  return DataUri(
      "<!doctype html><title>ready</title><input id=f>"
      "<div style=\"height:5000px\"></div>"
      "<script>addEventListener('keydown', e => { document.title = 'key:' + "
      "e.key + ':' + (document.activeElement.id || 'body'); }, true);"
      "addEventListener('scroll', () => { const y = scrollY;"
      " const max = document.scrollingElement.scrollHeight - innerHeight;"
      " document.title = 'scroll:' + (y > 0 && y >= max - 1 ? 'bottom'"
      " : Math.abs(y - innerHeight / 2) <= 1 ? 'half' : Math.round(y)); });"
      "</script>",
      "text/html");
}

void CefBrowserClient::SendTestKey(CefRefPtr<CefBrowser> browser,
                                   TestKey key,
                                   TestKeyPhase phase) {
  CefRefPtr<CefBrowserHost> host = browser->GetHost();
  // Keys for the open command line must reach it; focusing the page would
  // blur and close it.
  if (!CommandLineOpen()) {
    host->SetFocus(true);
  }
#if defined(OS_MAC)
  // CefBrowserHost::SendKeyEvent trips a Chromium DCHECK for windowed
  // browsers on macOS, so send a native event instead.
  PlatformSendTestKey(browser, key, phase);
#else
  // Linux windowed CEF reports X11 keycodes (evdev code + 8), not macOS
  // virtual key codes. A focused field drops a key whose native code is
  // wrong even when the same event reaches the page body. Windows uses the
  // virtual key as the native code; this path has not been run there.
  CefKeyEvent event;
  event.windows_key_code = key.windows_key_code;
#if defined(OS_LINUX)
  event.native_key_code = key.linux_key_code;
#else
  event.native_key_code = key.windows_key_code;
#endif
  event.modifiers = (key.shift ? EVENTFLAG_SHIFT_DOWN : 0) |
                    (phase == TestKeyPhase::kRepeat ? EVENTFLAG_IS_REPEAT : 0);
  event.character = key.character;
  event.unmodified_character =
      key.unshifted != 0 ? key.unshifted
      : key.shift && key.character >= u'A' && key.character <= u'Z'
          ? static_cast<char16_t>(key.character - u'A' + u'a')
          : key.character;
  if (phase != TestKeyPhase::kUp) {
    event.type = KEYEVENT_RAWKEYDOWN;
    host->SendKeyEvent(event);
    if (key.character >= 0x20) {
      // Linux CHAR events carry the character in windows_key_code, not the
      // virtual key. Sending the virtual key here skips the DOM keydown once
      // an editor is focused.
      event.type = KEYEVENT_CHAR;
      event.windows_key_code = key.character;
      host->SendKeyEvent(event);
      event.windows_key_code = key.windows_key_code;
    }
  }
  if (phase == TestKeyPhase::kPress || phase == TestKeyPhase::kUp) {
    event.type = KEYEVENT_KEYUP;
    host->SendKeyEvent(event);
  }
#endif
}

void CefBrowserClient::SendTestHold(CefRefPtr<CefBrowser> browser,
                                    TestKey key,
                                    int repeats,
                                    bool release,
                                    int64_t probe_delay_ms,
                                    const std::string& probe) {
  auto at = [&](int64_t ms, TestKeyPhase phase) {
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&CefBrowserClient::SendTestKey, this,
                                      browser, key, phase),
                       ms);
  };
  at(0, TestKeyPhase::kDown);
  int64_t ms = 100;
  for (int i = 0; i < repeats; ++i, ms += 50) {
    at(ms, TestKeyPhase::kRepeat);
  }
  if (release) {
    at(ms, TestKeyPhase::kUp);
  }
  CefPostDelayedTask(
      TID_UI,
      base::BindOnce(&CefBrowserClient::RunTestScript, this, browser, probe),
      ms + probe_delay_ms);
}

void CefBrowserClient::RunTestScript(CefRefPtr<CefBrowser> browser,
                                     const std::string& script) {
  browser->GetMainFrame()->ExecuteJavaScript(script, "", 0);
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
  // From step 7 on every key is a bound motion, so the page must see none.
  constexpr int kFirstScrollStep = 7;
  constexpr int kHoldStep = 14;
  if (event == "title key:r:body" || event.starts_with("title key:Escape") ||
      event == "title timeout:running" ||
      (event.starts_with("title hold:") &&
       event != "title hold:moved:stopped") ||
      (key_smoke_step_ >= kFirstScrollStep &&
       event.starts_with("title key:"))) {
    std::fprintf(stderr, "vrowser-smoke: keys failed, page saw %s\n",
                 event.c_str());
    FinishSmoke(1);
    return;
  }
  struct Step {
    const char* wait_for;
    const char* note;
    // Keys typed once `wait_for` arrives, in order. Steps 2, 3, and the last
    // step act differently; see the switch below.
    TestKey keys[3];
    int key_count;
  };
  static constexpr Step kSteps[] = {
      {"load-end", "page loaded; unbound q goes to the page", {kTestQ}, 1},
      {"title key:q:body",
       "bound r reloads and stays out of the page",
       {kTestR},
       1},
      {"load-end", "reloaded; focus the text field", {}, 0},
      {"mode insert", "Insert mode; r goes to the field", {kTestR}, 1},
      {"title key:r:f", "Escape leaves Insert", {kTestEscape}, 1},
      {"mode normal",
       "Normal mode; q goes to the page, Escape did not",
       {kTestQ},
       1},
      {"title key:q:f", "j scrolls one line step", {kTestJ}, 1},
      {"title scroll:60", "3j scrolls three line steps", {kTest3, kTestJ}, 2},
      {"title scroll:240", "k scrolls up one line step", {kTestK}, 1},
      {"title scroll:180", "G scrolls to the bottom", {kTestShiftG}, 1},
      {"title scroll:bottom", "gg scrolls to the top", {kTestG, kTestG}, 2},
      {"title scroll:0", "d scrolls half a page down", {kTestD}, 1},
      {"title scroll:half", "u scrolls half a page up", {kTestU}, 1},
      {"title scroll:0",
       "jjj typed quickly ends where 3j does",
       {kTestJ, kTestJ, kTestJ},
       3},
      {"title scroll:180",
       "hold j: the page keeps scrolling, and stops on key-up",
       {},
       0},
      {"title hold:moved:stopped",
       "hold j with no key-up: the loop stops on its own",
       {},
       0},
      {"title timeout:stopped", "load a second page", {}, 0},
      // Wait for each load to finish: a history key pressed mid-load is lost.
      {"load-end", "second page loaded; H goes back", {kTestShiftH}, 1},
      {"load-end",
       "back on the first page; H at the start of history does nothing; L "
       "goes forward",
       {kTestShiftH, kTestShiftL},
       2},
      {"title second", "done", {}, 0},
  };
  constexpr int kStepCount = static_cast<int>(std::size(kSteps));
  if (key_smoke_step_ >= kStepCount ||
      event != kSteps[key_smoke_step_].wait_for) {
    return;
  }
  const int index = key_smoke_step_++;
  const Step& step = kSteps[index];
  std::fprintf(stderr, "vrowser-smoke: step %d %s: %s\n", index, event.c_str(),
               step.note);
  if (index == kStepCount - 1) {
    std::fprintf(stderr, "vrowser-smoke: keys ok\n");
    FinishSmoke(0);
    return;
  }
  if (index == 2) {
    browser->GetMainFrame()->ExecuteJavaScript(
        "document.getElementById('f').focus()", "", 0);
    return;
  }
  // The probes record scrollY, wait 300 ms, and report whether the page moved
  // well past where the hold began (180) and whether it is still moving.
  if (index == kHoldStep) {
    SendTestHold(browser, kTestJ, 10, true, 100,
                 "(() => { const y = scrollY; setTimeout(() => { "
                 "document.title = 'hold:' + (y > 400 ? 'moved' : 'short') + "
                 "':' + (scrollY === y ? 'stopped' : 'running'); }, 300); "
                 "})();");
    return;
  }
  if (index == kHoldStep + 2) {
    // Chromium ignores a page's own fragment navigation on a top-level data:
    // URL, so the second history entry is a browser-initiated load.
    browser->GetMainFrame()->LoadURL(
        DataUri("<!doctype html><title>second</title>", "text/html"));
    return;
  }
  if (index == kHoldStep + 1) {
    // No key-up: wait past the loop's own deadline before probing.
    SendTestHold(browser, kTestJ, 6, false, kHeldScrollTimeoutMs + 200,
                 "(() => { const y = scrollY; setTimeout(() => { "
                 "document.title = 'timeout:' + (scrollY === y ? 'stopped' : "
                 "'running'); }, 300); })();");
    return;
  }
  // OnFocusedNodeChanged can arrive before the editor is ready to take a key.
  // Sending immediately is flaky on Linux: the event is forwarded but never
  // becomes a DOM keydown. A short delay after focus lets it settle.
  const int64_t delay_ms = index == 3 ? 200 : 0;
  for (int i = 0; i < step.key_count; ++i) {
    CefPostDelayedTask(
        TID_UI,
        base::BindOnce(&CefBrowserClient::SendTestKey, this, browser,
                       step.keys[i], TestKeyPhase::kPress),
        delay_ms);
  }
}

}  // namespace vrowser
