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
client registers `reload`, bound to `r` in Normal mode. `--url` is a process switch parsed in
`src/core`, not a command.

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
  Mode and pending keys are kept per tab.
- Editable focus. The renderer's `CefRendererApp::OnFocusedNodeChanged` sends
  `vrowser.editable-focus` with one bool. The browser accepts only that message
  shape from `PID_RENDERER` and uses it only to switch that tab between Normal
  and Insert. Focus inside cross-process iframes is #23.
- One default binding: `r` runs the `reload` command through the
  `CommandRegistry`. Scrolling bindings are #19.
