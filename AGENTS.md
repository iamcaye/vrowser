# AGENTS.md

This file is the source of truth for a coding agent that has no previous
conversation about Vrowser. Read it before changing the repository. Then read
`docs/progress/current-status.md`.

## Project vision

Vrowser is an open-source, cross-platform, keyboard-first web browser. It is
meant to become a daily driver in the spirit of Vim and Neovim: modal input,
composable commands, and a small trusted shell around a current Chromium.

The browser must stay fast, compatible with the modern web, and maintainable
by people and by agents working one issue at a time. Remote page content is
untrusted. The Chromium sandbox stays on.

## Technology stack

- C++20.
- Official Chromium Embedded Framework binaries. The pin is
  `cmake/cef_version.cmake`. Do not build Chromium from source unless an issue
  says so.
- CMake 3.24+ and `CMakePresets.json`.
- CEF Views for the window. Native windows are available with `--use-native`.
- SQLite is the store. `src/storage` builds the pinned official amalgamation
  (`vrowser_sqlite`) and runs schema migrations. No tables exist yet.
- GoogleTest and CTest.
- clang-format (Chromium style) and clang-tidy.
- GitHub Actions for portable unit tests.
- Platforms: macOS, Linux, and Windows. The CEF app has been built on macOS
  arm64. Linux and Windows sources follow the current cefsimple entry points
  and still need a build on those hosts.

Vrowser does not use Electron, Node.js, or a web UI framework in the shell.

## Architecture overview

CEF 156 uses the Chrome bootstrap. The Alloy bootstrap was removed in M128.
Chrome style and Alloy style are per-window choices on that bootstrap. They do
not implement the same callbacks. See `docs/architecture/process-model.md` and
`docs/rfcs/0001-architecture.md`.

The prototype opens one Views window, loads a URL, and shuts down through
CEF's close sequence. Domain code that does not need CEF lives in `src/core`
and must stay free of CEF headers. The CEF adapter lives in `src/browser/cef`.

```text
apps/desktop     process entry, bundle, helpers
src/core         startup configuration
src/browser/cef  CefApp, CefClient, settings, window
include/vrowser public headers that do not include CEF
tests/unit       GoogleTest, no CEF
```

Planned modules (`src/vim`, `src/commands`, `src/ui`, `src/storage`,
`src/config`) have no runtime yet. Do not add empty class hierarchies for
them. Add code when an issue implements the behavior.

## Module responsibilities

| Module | Responsibility | Depends on |
| --- | --- | --- |
| `src/core` | Startup switches, paths, later session and profile models | C++ standard library |
| `src/browser/cef` | CEF lifecycle, windows, load events | `src/core`, CEF |
| `src/browser/interfaces` | Future browser-neutral navigation API | `src/core` only |
| `src/vim` | Modes, key sequences, counts | `src/core`, commands |
| `src/commands` | Command registry and parser | `src/core` |
| `src/ui` | Tabs, status, hints, command line | browser interface, vim |
| `src/storage` | SQLite migrations, history, bookmarks, sessions | `src/core` |
| `src/config` | User keymaps and settings files (`vrowserrc`) | `src/core`, commands, vim |
| `apps/desktop` | `main`, macOS helpers, plist, Windows manifest | browser/cef |

## Dependency boundaries

- `include/vrowser` and `src/core` must not include CEF headers.
- `src/vim` and `src/commands` must be testable without opening a window.
- CEF types do not cross into storage or vim code. Translate at the adapter.
- Do not call CEF APIs off the thread documented on that API. UI-thread
  callbacks use `CEF_REQUIRE_UI_THREAD()`.
- Own CEF objects with `CefRefPtr`. Do not store a second owning raw pointer.
- The desktop target is compiled with CEF's flags: C++20, no exceptions, no
  RTTI, warnings as errors. `startup_config.cpp` is compiled into that target,
  so it must also be valid under those flags.
- On macOS the executable loads the CEF framework at runtime. Do not link
  `libcef` into the macOS app. The sandbox requires that.

## Build and test

Requirements: CMake 3.24+, a C++20 compiler, Ninja, and network access the
first time GoogleTest or CEF is fetched. macOS builds of the pinned CEF need
the Xcode listed for that branch (Xcode 27 for CEF 156).

[`just`](https://github.com/casey/just) wraps the commands below. Run `just`
to list recipes. `just test` runs the unit tests, `just run [url]` builds and
launches the macOS app with a profile in `build/dev-profile`, and `just smoke`
runs the smoke check. The raw CMake commands still work without `just`.

```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure

cmake --preset debug
cmake --build --preset debug
```

The macOS app is:

```text
build/debug/apps/desktop/Debug/Vrowser.app
```

Run it:

```bash
open build/debug/apps/desktop/Debug/Vrowser.app
# or, with a chosen URL and profile:
build/debug/apps/desktop/Debug/Vrowser.app/Contents/MacOS/Vrowser \
  --url=https://example.com \
  --user-data-dir=/tmp/vrowser-profile
```

Smoke check (opens a window, loads the URL, then quits):

```bash
tools/smoke_test.sh
```

macOS bundle layout (main executable, five helpers, CEF framework):

```bash
tools/check_mac_bundle.sh
# or, after a debug build:
cmake --build --preset debug --target check_mac_bundle
```


Switches:

- `--url=<url>` initial page. Default `https://example.com`.
- `--user-data-dir=<path>` CEF root cache. Default is the platform user-data
  directory documented in `docs/architecture/process-model.md`.
- `--profile=<name>` profile in that directory (`default` when omitted). Each
  profile has its own cookies, cache, and `vrowser.db`.
- `--use-alloy-style` Alloy window instead of the default Chrome style.
- `--use-native` platform window instead of Views.
- `--vrowser-smoke-test` quit after the main frame load.
- `--vrowser-tab-smoke-test` open a second tab, close the first, and quit.
  Needs `--use-alloy-style`. `just tab-smoke` runs it.
- `--vrowser-key-smoke-test` send keys to a built-in page and check Normal and
  Insert modes and the scroll bindings. Chrome style only. `just key-smoke` runs it.

Format:

```bash
clang-format -i $(git ls-files '*.cc' '*.cpp' '*.h' '*.hpp' '*.mm')
```

`clang-tidy` is configured in `.clang-tidy`. Run it on changed Vrowser files.
Do not tidy `third_party/` or the CEF wrapper.

## Coding standards

- C++20. Apache-2.0 header on new source files: `SPDX-License-Identifier: Apache-2.0`.
- Chromium clang-format. Comments and docs are English.
- RAII. No owning raw pointers. `CefRefPtr` for CEF ref-counted objects.
- Keep functions small enough to review. Match the surrounding file.
- Warnings are errors on the desktop target. Fix them. Do not disable a
  warning to land a feature.
- Tests cover behavior that does not need a window. Say so when a check could
  not be run, and name the platform.

## Security requirements

- Treat HTML, URLs, headers, and IPC from the renderer as untrusted.
- Leave `CEF_USE_SANDBOX` defined. The CMake option `USE_SANDBOX` defaults to
  ON. A build that sets `-DUSE_SANDBOX=OFF` is a local debugging build, not a
  release.
- Do not add `--disable-web-security`, a custom scheme with the same privileges
  as `https`, or a way for page JavaScript to reach the filesystem.
- Set `root_cache_path` to a Vrowser-specific absolute directory. CEF uses it
  as the process singleton key.
- macOS helpers must stay inside the app bundle and keep the CEF framework
  layout produced by `COPY_MAC_FRAMEWORK`.
- Report vulnerabilities using `SECURITY.md`.

## Current development phase

M0 (foundation) and the first M1 slice (one window, one URL, clean shutdown)
are the active work. Plugins are not started, and storage has only the migration runner.
Read `docs/progress/current-status.md` for what was actually built and tested.

## Issue workflow

GitHub is the backlog. Milestones:

- M0 — Project Foundation
- M1 — CEF Browser Prototype
- M2 — Vim Navigation Engine
- M3 — Browser Essentials
- M4 — Extensibility

Labels: `type:*`, `priority:*`, `area:*`, plus `agent-ready`, `blocked`, and
`needs-review`.

Every issue includes background, scope, files, dependencies, acceptance
criteria, tests, out of scope, and the definition of done. `tools/github/bootstrap_github.py`
creates milestones, labels, and issues. It is idempotent.

Take an issue only when it is `agent-ready`, its dependencies are done, and
no open pull request already mentions it.

## Pull request conventions

- Branch: `feat/<issue>-<slug>`, `fix/<issue>-<slug>`, or `chore/<issue>-<slug>`.
- One issue per pull request unless the issue says otherwise.
- The PR body lists what changed, how it was verified, and what was not run.
- Use `Closes #<number>` when the PR fully meets that issue's acceptance
  criteria.
- Do not merge your own pull request. Do not force-push `main`.
- Update `docs/progress/current-status.md` when the user-visible state or the
  next recommended task changes.

## Definition of Done

- Acceptance criteria are checked against a build, not against intention.
- Tests named by the issue were added or updated, and the commands that passed
  are recorded.
- `clang-format` has been applied to the edited C++ files.
- The sandbox and CEF threading rules still hold.
- Docs that the change makes false are updated.
- The PR describes remaining blockers.

## How to pick the next task

1. Read this file and `docs/progress/current-status.md`.
2. Inspect `git status`, the current branch, and recent commits.
3. Inspect open issues and pull requests (`gh issue list`, `gh pr list`).
4. Skip issues that are `blocked`, missing `agent-ready`, or named by an open
   PR.
5. Choose the smallest `priority:p0` issue whose dependencies are merged.
   If none, choose the smallest `agent-ready` `priority:p1` issue.
6. Create the branch from the default branch and implement only that issue.
7. If blocked, comment on the issue, add `blocked`, remove `agent-ready`, and
   pick a different issue.

Do not start a plugin runtime, a custom UI toolkit, or a Chromium fork as a
side effect of a small issue.
