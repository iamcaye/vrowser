# RFC 0001 — Foundation architecture

- Status: accepted for the prototype
- Date: 2026-11-19

## Problem

Vrowser needs a Chromium-compatible engine, a native window, and a place for
a keyboard-first UI, without taking on a web-technology shell or a from-source
Chromium build.

## Decision

Use the official CEF 156.0.30 binary distribution (Chromium 156.0.8078.96,
Extended channel). Drive it with a small C++20 application. Keep startup
policy in `src/core`, which does not include CEF. Keep every CEF type in
`src/browser/cef` and `apps/desktop`.

Windowing is CEF Views. The process uses the Chrome bootstrap, which is the
only bootstrap in CEF 128 and later. The first window uses the default
runtime style, which resolves to Chrome style. `--use-alloy-style` selects
Alloy style. `--use-native` selects a platform window.

The profile directory is `CefSettings.root_cache_path`. The sandbox stays on.

## Options considered

**CEF Views, chosen.** One window implementation for macOS, Linux, and
Windows, already used by `cefsimple`. No Qt dependency. Chrome-style and
Alloy-style windows are both created through Views delegates.

**Qt Widgets.** A familiar native shell, and another large dependency that
would sit beside Chromium's own UI stack. Rejected until Views proves
insufficient for the Vim UI.

**Raw AppKit, Win32, and GTK.** Maximum control of the keyboard path, and
three window implementations to keep correct. Available later through
`--use-native` if a measured problem shows up. Not the default.

**Alloy bootstrap.** Removed from CEF in M128. It is not an option on the
pinned build.

**Chrome style as the long-term UI.** Chromium draws the toolbar, the
permission prompts, and most dialogs. That gets a real browser on screen
quickly. It also fights a custom command line and status bar. Chrome style is
the prototype default because it is the path `cefsimple` ships and because
Alloy style does not automatically provide the same permission UI.

**Alloy style as the long-term UI.** The client owns the window. That matches
a Vim shell. Alloy and Chrome do not implement the same callbacks, and
windowless rendering is Alloy-only. Adopting Alloy as the default waits for a
written comparison on the three desktop platforms. That comparison is the
next research issue.

**Build Chromium from source.** Required only if Vrowser must patch the
engine. The binary distribution is enough for the foundation.

## Consequences

- macOS ships helper bundles and loads the CEF framework at runtime. The app
  does not link `libcef`.
- Linux and Windows re-enter the same executable for subprocesses. Those
  entry points are in the tree and have not been compiled on those hosts.
- A browser-neutral interface is not introduced until a command needs to
  navigate without including CEF.
- Plugins wait until the window, navigation, and profile story work.
- CEF upgrades are a pin change in `cmake/cef_version.cmake` plus a smoke test.
  Hashes are recorded per platform.

## References

- https://chromiumembedded.github.io/cef/branches_and_building.html
- https://chromiumembedded.github.io/cef/architecture.html
- `include/internal/cef_types_runtime.h` in the pinned distribution
