# Architecture overview

Vrowser is a native C++ shell around the Chromium Embedded Framework. The
shell owns startup, windows, and later keyboard commands. Chromium owns
networking, rendering, the sandbox, and site isolation.

## Shape

```text
desktop process
  apps/desktop/main_*          CEF startup and the platform run loop
  src/core                     switches and paths, no CEF types
  src/browser/cef              CefApp, CefClient, Views window
  CEF framework                browser, network, GPU, renderer, sandbox

future, not in the prototype
  src/commands                 named operations
  src/vim                      modes and key sequences
  src/ui                       tabs, status line, link hints
  src/storage                  SQLite
  src/config                   user keymaps and settings
```

The prototype has one browser window. It does not yet have a tab model, a
command parser, or a database.

## Why this split

CEF's types, threads, and reference counting are easy to leak into the rest of
the program. `src/core` is compiled and tested without CEF so that startup
rules can be changed without a browser. The adapter in `src/browser/cef` is
the only place that includes `include/cef_*.h`.

A second browser backend is not planned. The interface directory stays empty
until a command or test needs to drive navigation without including CEF. That
is recorded in RFC 0001.

## Windowing

The window is a CEF Views `CefWindow` hosting a `CefBrowserView`. Views is
part of the CEF binary, so the prototype does not take a Qt or AppKit
dependency of its own. `--use-native` asks CEF to create the platform window
instead. That path exists because cefsimple supports it. The tested path is
Views on macOS.

Chrome style is the default runtime style. `--use-alloy-style` selects Alloy
style. The two styles are not interchangeable. Chrome style supplies Chromium's
own toolbar and dialogs. Alloy style gives the client more of the window and
is the planned Vim-shell target. The macOS comparison and recommendation are
in [runtime-style.md](runtime-style.md).

## Process and data

Each profile is a directory passed to `CefSettings.root_cache_path`. CEF locks
that directory so a second launch is delivered to the running process through
`OnAlreadyRunningAppRelaunch`. The prototype shows the existing window and, when
the new launch passed `--url`, records that URL in the core
`NavigationController`. The CEF client listens to the controller and calls
`LoadURL` on the UI thread. Later commands use the same controller, so they
navigate without including CEF.

Page content, renderers, and GPU run in CEF's helper processes. On macOS those
helpers are separate app bundles inside `Vrowser.app`. On Linux and Windows the
same executable is re-entered through `CefExecuteProcess`.

## Extension points that are intentionally absent

Plugins, user script injection, and Chrome extension APIs are M4 work. They
need a sandbox story of their own. The foundation does not reserve a plugin
loader. The proposed plugin API and its trust boundary are in
[RFC 0002](../rfcs/0002-plugins.md); no runtime exists.
