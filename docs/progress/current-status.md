# Current status

Updated: 2026-11-22

## Milestone

M0 (project foundation) and the first slice of M1 (one CEF window that loads
a page) are implemented locally. Vim input, tabs, SQLite, and plugins are not
started.

## Implemented

- CMake presets `ci`, `debug`, and `release`. `ci` builds unit tests without
  downloading CEF.
- C++20 library `vrowser_core` with startup-switch parsing and a
  `NavigationController` the CEF client loads from on the UI thread.
- C++20 library `vrowser_commands` with a command registry and a
  whitespace command-line parser. No CEF headers.
- C++20 library `vrowser_vim` with the mode state machine (Normal, Insert,
  Command, Hint) and `NormalizeKey`, which maps CEF-shaped key events to one
  `KeyEvent` type, and `KeyBindings`, a per-mode table from Vim-notation key
  sequences to command names, with Normal-mode counts (`12j`, `5gg`). No CEF
  headers.
- GoogleTest tests for startup switches, the command registry, mode
  transitions, key normalization, key bindings, counts, navigation
  requests, the tab list, and key dispatch.
- A second launch with `--url` loads that URL in the running window.
- Core `TabList` and tabs in the main window (Alloy style only): each tab is a
  browser view under a fill layout, only the active one is visible, closing a
  tab removes its view, and closing the last tab quits. `t` opens a tab
  with the start page, `gt`/`gT` switch tabs and wrap, `x` closes the current
  tab. No tab strip yet.
- Normal and Insert modes in the window: Normal-mode bindings are consumed
  before the page sees them, a focused text field switches the tab to Insert,
  and Escape returns to Normal. Bindings: `r` reloads, `j`/`k` scroll a line
  step, `d`/`u` half a page, `gg`/`G` to the top and bottom of the focused
  frame. A count repeats `r` (`3r`) and multiplies a scroll step (`5j`).
  OS key repeat does not repeat a binding; holding `j`, `k`, `d`, or `u`
  keeps scrolling until the key is released.
  A main-frame load drops consumed key-up tracking, and editable focus clears
  a pending key sequence.
- clang-format (C++ and Objective-C), clang-tidy config, editorconfig.
- GitHub Actions workflow that runs the unit tests on Linux, macOS, and
  Windows. Run [44557827020](https://github.com/iamcaye/vrowser/actions/runs/44557827020)
  completed successfully.
- GitHub milestones M0–M4, labels, and the backlog. Foundation work is closed
  in issues #1–#7. `tools/github/bootstrap_github.py` recreates anything
  missing.
- Pinned CEF 156.0.30+g6028375+chromium-156.0.8078.96, Extended channel.
  SHA256 hashes for the official platform archives are in
  `cmake/cef_version.cmake`.
- Desktop shell: Chrome bootstrap, CEF Views, default Chrome runtime style,
  `--use-alloy-style`, `--use-native`, profile directory, macOS helper
  bundles, clean shutdown through `CefQuitMessageLoop`.
- `tools/smoke_test.sh` loads a URL and exits.
- Runtime-style comparison on macOS:
  [docs/architecture/runtime-style.md](../architecture/runtime-style.md).
  Chrome style stays the prototype default. Alloy style is the planned Vim
  shell target once permission, certificate, download, and find UI are owned
  by Vrowser.

## Verification

Host: macOS 26.6.2, arm64, Xcode 27.0 (build 27A266a), CMake 4.5.0, Apple
Clang 21.0.0, Ninja 1.13.2.

| Check | Result |
| --- | --- |
| `cmake --preset ci && cmake --build --preset ci && ctest --preset ci` | 127 tests passed |
| `ctest --test-dir build/debug` | 9 tests passed (older local debug tree) |
| `cmake --preset debug && cmake --build --preset debug` | Built `Vrowser.app` and five helper bundles |
| CEF macOS arm64 archive SHA256 | `22a510e5d348a907953278b618e1e9e442ccf78773b9fee0e09eaf154eef53b0` |
| Smoke test, sandbox left on | `https://example.com` main frame HTTP 200, process exit 0 |
| Relaunch with `--url=https://www.iana.org/help/example-domains` into a running app | `vrowser: load-end 200 https://www.iana.org/help/example-domains` in the first process |
| `just tab-smoke` (`--vrowser-tab-smoke-test --use-alloy-style`) | `t` opened the second tab; `gt` wrapped to tab 1, `gT` wrapped to tab 2; `x` closed tab 2 (`vrowser-smoke: tabs ok, 1 open, remaining visible`); `x` on the last tab quit, exit 0 |
| `--vrowser-tab-smoke-test` in Chrome style | `vrowser: tabs need --use-alloy-style`, exit 1 (second Chrome-style browser view is never created) |
| `just key-smoke` (`--vrowser-key-smoke-test`, Chrome style) | unbound `q` reached the page, `r` ran `reload` and did not, focused field selected Insert, Escape returned to Normal and did not reach the page; `j` 60, `3j` 240, `k` 180, `G` bottom, `gg` 0, `d` half a viewport, `u` 0, no motion key reached the page; held `j` kept scrolling and stopped on key-up, held `j` with no key-up stopped on its own; `vrowser-smoke: keys ok`, exit 0 (three runs) |
| `--vrowser-key-smoke-test --use-alloy-style` | not supported: synthesized keys never reach Alloy pages, even without a keyboard handler; exits 1 with a message |
| Real keyboard, Chrome style and `--use-alloy-style` (manual, `https://duckduckgo.com`) | `r` reloads and stays out of the page, a focused field selects Insert and types `r`, Escape returns to Normal and does not reach the field, the next `r` reloads; `3r` reloads three times; a held `r` reloads once; Cmd+L and unbound keys still reach the browser and page |
| Scroll bindings, real keyboard, Chrome style and `--use-alloy-style` (manual, long Wikipedia page) | `j`/`k` move a line step, `5j` five steps, `d`/`u` half a page, `G` bottom, `gg` top; `j` in a focused field types and does not scroll, and scrolls again after Escape |
| Held scroll, real keyboard, Chrome style and `--use-alloy-style` (manual, long page) | held `j`/`k`/`d`/`u` scroll continuously and stop on release; a short `j` moves one step; held `r` reloads once; Cmd+Tab away while holding `j` stops within a second |
| Chrome-style smoke (`--vrowser-smoke-test`) | exit 0, `vrowser-smoke: ok 200` |
| Alloy-style smoke (`--use-alloy-style --vrowser-smoke-test`) | exit 0, `vrowser-smoke: ok 200` |
| `https://expired.badssl.com/` Chrome style | Chromium keeps the error UI; no client data URL |
| `https://expired.badssl.com/` Alloy style | Client data-URL error page via `OnLoadError` |
| `open build/debug/apps/desktop/Debug/Vrowser.app --args --url=https://example.com` | Launched |
| CEF Linux workflow (`cef-linux.yml` on `ubuntu-latest`) | Built `vrowser` and `chrome-sandbox`; sandbox left ON |
| CEF Windows workflow (`cef-windows.yml` on `windows-latest`) | Built `vrowser.exe` (bootstrap) and `vrowser.dll`; sandbox left ON |
| `tools/check_mac_bundle.sh` against debug `Vrowser.app` | passed |
| `tools/check_mac_bundle.sh` against missing-helper fixture | failed as expected |

The passing smoke command was `tools/smoke_test.sh` after the macOS keychain
permission was available. It did not pass `--use-mock-keychain`.

Linux and Windows CEF CI workflows are on `main`.

Linux desktop (Ubuntu 24.04, x86_64, Clang 18, CEF 156 linux64, sandbox on),
checked on this branch after the key-dispatch fixes:

| Check | Result |
| --- | --- |
| `just test` | 107 tests passed |
| `just smoke` | exit 0, `vrowser-smoke: ok 200 https://example.com/` |
| `just tab-smoke` | exit 0, `vrowser-smoke: tabs ok, 1 open, remaining visible` |
| `just key-smoke` | exit 0, `vrowser-smoke: keys ok` (Chrome style, synthesized keys) |
| Real X11 keys, Chrome style and `--use-alloy-style` | `r` reloads and stays out of the page, `x` reaches the page, a focused field types `r` including keyup, Escape does not reach the page, the next `r` reloads |
| Held `r` in Normal mode for half a second | one `vrowser: command reload` |

## Known limitations

- CEF logs `code_sign_clone_manager.mm` failing to clear a quarantine
  attribute on a temporary directory. The page load still completed.
- During shutdown CEF can log `waitpid: No child processes` and a GPU
  shared-image error. The process still returned 0 on the verified run.
- Chrome style shows Chromium's toolbar. The Vim shell should move to Alloy
  style after the UI gaps listed in
  [runtime-style.md](../architecture/runtime-style.md) are closed.
- No tab strip, command line, history, or bookmarks.
- On macOS a key-down consumed in `OnPreKeyEvent` never gets its key-up
  there; a held scroll is stopped by an `NSEvent` key-up monitor, and other
  consumed key-ups are dropped from tracking on the next press.
- Scrolling moves the focused frame's document. A page that scrolls an inner
  element instead of the document does not move, and a focused iframe that
  cannot scroll does not hand the motion to its parent.
- The macOS helpers are ad-hoc signed by the linker. A release package still
  needs a real signing and notarization pass.
- GoogleTest is fetched as the v1.16.0 tag. The desktop shell is pinned by
  SHA256. The test archive is not.

## Decisions

Recorded in [docs/rfcs/0001-architecture.md](../rfcs/0001-architecture.md).
CEF 156 was selected because it is the current Extended branch and this
machine matches its Xcode 27 / macOS 26.6 build requirement.

Runtime style: keep Chrome as the prototype default; target Alloy for the
Vim shell. Details in
[docs/architecture/runtime-style.md](../architecture/runtime-style.md).

## Next unblocked work

Normal and Insert modes work in the window (#18), Normal mode scrolls the
page (#19), and `t`/`gt`/`gT`/`x` open, switch, and close tabs (#32, #58). Following focus into iframes (#23) builds on
the same dispatcher. Real typing was
checked by hand on macOS in both Chrome and Alloy styles. Permission handling should follow
Alloy's client-owned prompt path.

## Recommended next task

[Animate scroll motions](https://github.com/iamcaye/vrowser/issues/56).
