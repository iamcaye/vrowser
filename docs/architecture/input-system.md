# Input system

The prototype does not install a Vim keymap. This document records the design
the later issues will implement, so agents do not invent a second one.

## Modes

Four modes are enough for the first keyboard layer:

| Mode | Keys go to | Leaves the mode when |
| --- | --- | --- |
| Normal | the key-sequence parser | a complete binding runs, or the user enters another mode |
| Insert | the focused web page or a text field in the shell | Escape (or the configured equivalent) |
| Command | the command line parser | Enter runs the command, Escape cancels |
| Hint | the hint overlay | a hint is chosen, or Escape cancels |

Insert is the default when focus is in an editable element, a content-editable
region, or a selected text field inside a frame. Normal is the default
otherwise. The page can move focus at any time. The mode machine must follow
focus changes reported by CEF rather than assuming the last key decided the
mode.

## Events

Platform key events are normalized before they reach the parser. The normalized
event carries:

- a key identity that does not depend on the current layout's shifted
  character (`k`, `Enter`, `Escape`, `Tab`)
- the character produced, when there is one
- modifiers that mean Control, Alt, Meta, and Shift
- whether the event is a key-down that should start an action

CEF and the host platform both observe keys. The shell handles a key only when
it is in a mode that owns the keyboard, and then it consumes the event so the
page does not see it too. Insert mode forwards the event to the focused frame.

Multi-key sequences (`gg`, `gT`, a count followed by `j`) are parsed as a
prefix tree with a timeout. Counts are a prefix of digits. `0` is a command
when it is the first key and a digit when a count has already started. The
parser is a pure function from `(mode, buffer, key)` to `(buffer, action)` and
is unit-tested without CEF.

## Commands

A binding resolves to a command name plus arguments, not to a function pointer
buried in the UI. The command registry will live in `src/commands` and will be
callable from a key binding, the command line, and a later CLI. The browser
client registers `reload` and the scroll commands; `AddDefaultBindings` in
`src/vim` binds them in Normal mode. A command registered with
`RegisterCounted` takes the count itself (`5j` is one call), while a plain
command is repeated (`3r`). `--url` is a process switch parsed in
`src/core`, not a command.

The command line (Alloy main window) opens with `:` in Normal mode: the tab
enters Command mode and a one-line `CefTextfield` above the status bar takes
focus. Enter runs the line through `CommandRegistry::Execute`; Escape, or
focus leaving the line, closes it without running anything. Either way the
tab returns to Normal and the page gets focus back. An error such as
`Unknown command: foo` is shown in the status bar after the URL until the
next key press. `open <url>` loads a URL in the current tab through
`NavigationController`, which rejects empty and `javascript:` URLs; turning
text into a URL or a search is #64. Chrome style has no command line yet.

Punctuation keys (Windows OEM virtual keys) are named by their US-layout
character without Shift (`;`), and the binding notation uses the typed
character, so Shift+`;` on a US layout and Shift+`.` on a Spanish layout
both match `:`. Before this, those keys had no identity and were dropped.
A Windows raw key-down carries no character, so there `:` would read as
`<S-;>`; Windows has not been run.

## Hints

Hint mode labels visible links in the focused frame and in child frames that
are part of the same tab. Labels are generated from a configurable charset.
Typing a unique label activates the link. Hint mode does not inject a
persistent content script. The first implementation can use a short-lived
DOM query from the renderer process. That work waits until the browser can
create more than one tab and can report frame focus.

## What the prototype handles today

- macOS menu Quit, which starts the CEF close sequence.
- Dock reopen, which shows the existing window.
- A second launch with the same profile, which is routed by CEF to
  `OnAlreadyRunningAppRelaunch`.
- Normal and Insert in the window. `CefBrowserClient::OnPreKeyEvent`
  normalizes each key (`NormalizeKey`) and asks `DispatchKey` in `src/vim`
  whether to consume it. Normal mode consumes keys that complete or extend a
  binding and forwards the rest, so browser shortcuts still work. Insert mode
  forwards everything except Escape, which returns to Normal and is consumed.
  Mode and pending keys are kept per tab. A count (`3r`) is passed to
  `CommandRegistry::Run`, which repeats the command that many times. OS key
  repeat does not run the binding again. Instead `DispatchKey` reports a
  held press that completed a binding (`HoldEvent`: start on the first
  repeat, continue, stop on key-up), and the client keeps `j`, `k`, `d`, and
  `u` scrolling from one `requestAnimationFrame` loop in the page
  (`HeldScrollStartScript`), 900 px/s for lines and two viewports per second
  for half pages. Repeats push the loop's deadline out (throttled to one
  script per 150 ms); without them it stops on its own after 600 ms, so a lost
  key-up or a window that loses focus cannot leave the page scrolling. Key-up,
  editable focus, a main-frame load, a tab switch, and closing the tab stop
  it. macOS drops the key-up of a key-down that `OnPreKeyEvent` consumed, so
  there an app-local `NSEvent` key-up monitor (`PlatformWatchKeyUps`) stops
  the hold instead. A character event after a consumed
  key-down is consumed too. Editable-focus messages clear a pending sequence,
  and a main-frame load clears consumed key-up tracking.
- Editable focus. The renderer's `CefRendererApp::OnFocusedNodeChanged` sends
  `vrowser.editable-focus` with one bool. The browser accepts only that message
  shape from `PID_RENDERER` and uses it only to switch that tab between Normal
  and Insert. Focus inside cross-process iframes is #23.
- Default bindings: `r` reloads; `j`/`k` scroll one 60px line step, `d`/`u`
  half the viewport, `gg`/`G` to the top and bottom. Line and half-page
  motions multiply by the count; `gg` and `G` ignore it. A command runs in
  the browser whose key triggered it. Scrolling runs a constant script
  (`ScrollScript` in `src/core`) with `ExecuteJavaScript` in the focused
  frame, or the main frame when none has focus. It scrolls the frame's
  document, so a page that scrolls an inner element instead (some web apps)
  does not move yet. Each step eases out over `kScrollAnimationMs` (120 ms)
  from one `requestAnimationFrame` loop. A step that arrives mid-animation
  adds to the pending target, so `jjj` ends where `3j` does, and targets are
  clamped to the page. `prefers-reduced-motion: reduce` makes steps instant,
  and `kScrollAnimationMs = 0` restores instant scrolling until there is a
  setting. Steps and the held-key loop share one state object in the page,
  so each cancels the other.
- History keys: `H` goes back one entry and `L` forward one entry in the tab
  whose key ran them (`CefBrowser::GoBack`/`GoForward`); at either end the
  key is consumed and nothing happens. A count is ignored for now.
- Tab keys: `t` opens a new tab with the start page (the initial `--url`
  until there is a home page setting) and focuses it. `gt` and `gT` move to the next and previous tab and wrap; a count
  moves that many tabs (`3gt`). `x` closes the tab whose key ran it; closing
  the last tab quits. The tab that becomes active gets keyboard focus and the
  window title, and a key still held across the switch keeps its key-up out
  of the new page (`TransferHeldKeys`). Only the active tab sets the window
  title. Several tabs need Alloy style; in Chrome style `gt`/`gT` stay on the
  one tab and `x` quits.
