#!/usr/bin/env python3
"""Create Vrowser milestones, labels, and issues.

The script is idempotent. A second run does not create a second milestone,
label, or issue with the same title. Completed foundation issues are closed
when they already exist and are still open.

Usage:
  tools/github/bootstrap_github.py
  tools/github/bootstrap_github.py --repo iamcaye/vrowser
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys

DONE_COMMIT = "55d34258df4f5941ce73895b3790ad715fc0fefc"

MILESTONES = [
    (
        "M0 — Project Foundation",
        "CMake, tooling, CEF pin, CI, and the architecture docs.",
    ),
    (
        "M1 — CEF Browser Prototype",
        "A real window, navigation, tabs, and the Alloy versus Chrome decision.",
    ),
    (
        "M2 — Vim Navigation Engine",
        "Modes, key sequences, commands, scrolling, and link hints.",
    ),
    (
        "M3 — Browser Essentials",
        "SQLite, history, bookmarks, profiles, sessions, downloads, and permissions.",
    ),
    (
        "M4 — Extensibility",
        "Configuration, a local CLI, and a documented plugin boundary.",
    ),
]

LABELS = [
    ("type:feature", "1D76DB", "New behavior"),
    ("type:bug", "D73A4A", "Defect"),
    ("type:chore", "C5DEF5", "Tooling or maintenance"),
    ("type:docs", "0075CA", "Documentation"),
    ("type:research", "5319E7", "Investigation that ends in a written decision"),
    ("priority:p0", "B60205", "Do this before other open work"),
    ("priority:p1", "D93F0B", "Next after p0"),
    ("priority:p2", "FBCA04", "Later in the milestone"),
    ("area:core", "0052CC", "Process, session, and domain model"),
    ("area:cef", "0E8A16", "CEF adapter and Chromium integration"),
    ("area:vim", "6F42C1", "Modal input"),
    ("area:ui", "BFDADC", "Tabs, status, and overlays"),
    ("area:storage", "006B75", "SQLite and persisted data"),
    ("area:build", "FBCA04", "CMake, CI, and packaging"),
    ("area:security", "B60205", "Sandbox, permissions, and trust boundaries"),
    ("area:docs", "0075CA", "Architecture and contributor docs"),
    ("agent-ready", "0E8A16", "Unblocked and specified enough to implement"),
    ("blocked", "D93F0B", "Waiting on another issue or an external constraint"),
    ("needs-review", "C5DEF5", "Implementation is up for review"),
]


def body(
    background: str,
    scope: str,
    files: str,
    dependencies: str,
    acceptance: list[str],
    tests: str,
    out_of_scope: list[str],
    done: str,
) -> str:
    checks = "\n".join(f"- [ ] {item}" for item in acceptance)
    outside = "\n".join(f"- {item}" for item in out_of_scope)
    return f"""## Background and objective

{background}

## Technical scope

{scope}

## Suggested files or modules

{files}

## Dependencies

{dependencies}

## Acceptance criteria

{checks}

## Required tests

{tests}

## Out of scope

{outside}

## Definition of done

{done}
"""


DONE = (
    "The acceptance criteria are checked, the named tests were run or the "
    "pull request says which ones could not be run, and the sandbox and CEF "
    "threading rules still hold."
)

# Completed foundation work. Created and then closed.
CLOSED = [
    {
        "title": "Initialize the CMake project and C++20 presets",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:chore", "priority:p0", "area:build"],
        "body": body(
            "Vrowser needs a C++20 build that an agent can configure without choosing flags by hand.",
            "Add the top-level CMake project, `ci`, `debug`, and `release` presets, and the source layout for core, browser, vim, commands, ui, storage, and the desktop app.",
            "`CMakeLists.txt`, `CMakePresets.json`, `src/`, `apps/desktop/`, `include/vrowser/`.",
            "None. This is the first repository commit.",
            [
                "`cmake --preset ci` configures without CEF.",
                "`cmake --preset debug` configures the desktop shell on a supported host.",
                "The directory layout in the README matches the tree.",
            ],
            "Configure both presets. The `ci` preset must generate a unit-test target.",
            ["CEF window behavior", "Vim modes", "SQLite"],
            DONE,
        ),
    },
    {
        "title": "Add formatting, static analysis, and editor defaults",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:chore", "priority:p1", "area:build"],
        "body": body(
            "Agents and people need one formatting style so review comments stay about behavior.",
            "Add Chromium-based clang-format for C++ and Objective-C, a clang-tidy config that ignores `third_party/`, and EditorConfig.",
            "`.clang-format`, `.clang-tidy`, `.editorconfig`.",
            "Depends on the CMake project issue.",
            [
                "`clang-format --dry-run --Werror` passes on the C++ and Objective-C++ sources.",
                "clang-tidy is configured not to scan the CEF distribution.",
            ],
            "Run clang-format on the tree. clang-tidy does not have to be clean on the CEF wrapper.",
            ["A required clang-tidy CI job", "Formatting third-party CEF sources"],
            DONE,
        ),
    },
    {
        "title": "Add GoogleTest coverage for startup configuration",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:chore", "priority:p0", "area:core"],
        "body": body(
            "URL and profile-path parsing must be testable without starting CEF.",
            "Parse `--url`, `--user-data-dir`, `--use-alloy-style`, `--use-native`, and `--vrowser-smoke-test` in `vrowser_core`. Ignore unrelated CEF subprocess switches.",
            "`include/vrowser/core/startup_config.hpp`, `src/core/startup_config.cpp`, `tests/unit/startup_config_test.cpp`.",
            "Depends on the CMake project issue.",
            [
                "Default URL is `https://example.com`.",
                "Equals-form and following-argument forms of `--url` are covered.",
                "An empty `--url` keeps the default.",
                "CEF switches such as `--type=renderer` are ignored.",
            ],
            "`ctest --preset ci` runs the startup-config tests.",
            ["Loading the URL in a window", "Persisting the profile"],
            DONE,
        ),
    },
    {
        "title": "Add the portable unit-test GitHub Actions workflow",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:chore", "priority:p1", "area:build"],
        "body": body(
            "The unit tests have to run on Linux, macOS, and Windows without downloading CEF.",
            "Add a workflow that uses the `ci` preset and a format check.",
            "`.github/workflows/ci.yml`.",
            "Depends on the GoogleTest issue.",
            [
                "The workflow configures `VROWSER_WITH_CEF=OFF`.",
                "Ubuntu, macOS, and Windows jobs run `ctest`.",
                "A green run is recorded on the default branch.",
            ],
            "The workflow itself is the test. A local `ctest --preset ci` run is still required before the pull request.",
            ["A CI job that downloads CEF", "Release packaging"],
            DONE,
        ),
    },
    {
        "title": "Write architecture, agent, and contributor documentation",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:docs", "priority:p0", "area:core"],
        "body": body(
            "A new agent must be able to resume the project from the repository alone.",
            "Write `AGENTS.md`, the architecture notes, RFC 0001, the security policy, and the contributor guide.",
            "`AGENTS.md`, `docs/architecture/`, `docs/rfcs/0001-architecture.md`, `CONTRIBUTING.md`, `SECURITY.md`.",
            "Depends on the CMake and CEF decisions being chosen.",
            [
                "`AGENTS.md` describes how to pick the next issue.",
                "The process model matches CEF 156: Chrome bootstrap, per-window runtime style.",
                "RFC 0001 records the Views decision and the Alloy versus Chrome tradeoff.",
            ],
            "No automated test. Review the docs against `include/internal/cef_types_runtime.h` in the pinned distribution.",
            ["A plugin API", "User-facing help pages"],
            DONE,
        ),
    },
    {
        "title": "Pin the official CEF 156 binary distribution",
        "milestone": "M0 — Project Foundation",
        "labels": ["type:chore", "priority:p0", "area:cef", "area:build", "area:security"],
        "body": body(
            "The shell must link an official CEF build, not a from-source Chromium tree.",
            "Pin CEF 156.0.30 and record SHA256 hashes for the published platform archives. Fail configuration when the hash does not match.",
            "`cmake/cef_version.cmake`, `cmake/DownloadCEF.cmake`, `cmake/VrowserCef.cmake`.",
            "Depends on the CMake project issue.",
            [
                "The macOS arm64 archive hash is pinned and was verified locally.",
                "Hashes for the other official platform archives are in the version file.",
                "The downloaded tree is gitignored.",
            ],
            "Configure the `debug` preset and confirm CMake reports the pinned CEF root. Do not commit the archive.",
            ["Building Chromium", "A CEF version bump"],
            DONE,
        ),
    },
    {
        "title": "Open a CEF window and load a page",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:feature", "priority:p0", "area:cef"],
        "body": body(
            "The foundation has to show a real page, not a stub window.",
            "Initialize CEF with the sandbox left on, open one Views window, load the startup URL, and quit through `CefQuitMessageLoop` after the last browser closes.",
            "`apps/desktop/`, `src/browser/cef/`.",
            "Depends on the CEF pin issue.",
            [
                "The macOS app bundle contains the five helper bundles and the CEF framework.",
                "`tools/smoke_test.sh` reports HTTP 200 for `https://example.com` and the process exits 0.",
                "`root_cache_path` is a Vrowser-specific directory.",
            ],
            "The smoke script on macOS. Unit tests do not start CEF.",
            ["Tabs", "A custom toolbar", "Disabling the sandbox to make the smoke test pass"],
            DONE,
        ),
    },
]

OPEN = [
    {
        "title": "Compare Alloy style and Chrome style for the Vrowser shell",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:research", "priority:p0", "area:cef", "area:docs", "agent-ready"],
        "body": body(
            "The prototype uses Chrome style, so Chromium draws the toolbar. A Vim shell probably wants Alloy style, and the two styles do not implement the same callbacks. Pick the default before drawing Vrowser UI.",
            "Run the existing app on macOS with the default style and with `--use-alloy-style`. Compare the toolbar, permission prompts, certificate errors, popups, find-in-page, and shutdown. Write the recommendation in `docs/architecture/`. Change the default only if the notes say the prototype default is wrong and the change is a small switch.",
            "`docs/architecture/runtime-style.md`, `docs/progress/current-status.md`, `src/browser/cef/cef_runtime_app.cpp` only if the default changes.",
            "None. The window already loads a page on `main`.",
            [
                "Both styles launch on macOS with the sandbox enabled.",
                "The note lists behavior that exists in only one style.",
                "The note recommends the style for the future command line and status bar.",
                "`docs/progress/current-status.md` points at that recommendation.",
            ],
            "Manual: default launch and `--use-alloy-style` against `https://example.com`. Record the commands and the exit behavior. Re-run `tools/smoke_test.sh` if the default style changes.",
            [
                "A custom toolbar or status bar",
                "Keybindings",
                "Linux and Windows verification",
            ],
            DONE,
        ),
    },
    {
        "title": "Build and smoke-test the desktop shell on Linux",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:chore", "priority:p1", "area:build", "area:cef", "agent-ready"],
        "body": body(
            "Linux entry points exist and have not been compiled. A Linux agent needs a workflow that downloads the pinned CEF archive and builds the shell.",
            "Add `.github/workflows/cef-linux.yml`. It downloads the pinned `linux64` distribution, configures the `debug` preset, and builds the `vrowser` target. Keep the sandbox enabled. Do not edit the macOS bundle.",
            "`.github/workflows/cef-linux.yml`. Touch `apps/desktop/main_linux.cc` only to fix a compile error.",
            "None. Use a new workflow file so this does not collide with the Windows workflow issue.",
            [
                "The workflow uses the hash in `cmake/cef_version.cmake`.",
                "A run on `ubuntu-latest` builds `vrowser`.",
                "The pull request states whether a graphical smoke test was run.",
            ],
            "The new workflow is the build test. The existing unit-test workflow must stay green.",
            ["A graphical Linux smoke test in CI", "Changing the macOS app bundle", "Vim input"],
            DONE,
        ),
    },
    {
        "title": "Build and smoke-test the desktop shell on Windows",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:chore", "priority:p1", "area:build", "area:cef", "agent-ready"],
        "body": body(
            "Windows entry points follow cefsimple, including the sandbox bootstrap DLL, and have not been compiled.",
            "Add `.github/workflows/cef-windows.yml`. It downloads the pinned `windows64` distribution and builds the `vrowser` target with the Visual Studio environment. Leave `USE_SANDBOX` at its default.",
            "`.github/workflows/cef-windows.yml`. Touch `apps/desktop/main_win.cc` only to fix a compile error.",
            "None. Use a new workflow file so this does not collide with the Linux workflow issue.",
            [
                "The workflow uses the hash in `cmake/cef_version.cmake`.",
                "A run on `windows-latest` builds the target.",
                "The pull request states whether the sandbox bootstrap executable was produced.",
            ],
            "The new workflow is the build test. The existing unit-test workflow must stay green.",
            ["A graphical Windows smoke test in CI", "Changing the macOS app bundle", "Vim input"],
            DONE,
        ),
    },
    {
        "title": "Check the macOS app bundle layout",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:chore", "priority:p2", "area:build", "area:security", "agent-ready"],
        "body": body(
            "The sandbox depends on the helper bundles and the CEF framework being inside `Vrowser.app`. That layout is easy to break in a later CMake edit.",
            "Add a script that fails when the main executable, the five helper bundles, or `Chromium Embedded Framework.framework` is missing. Wire it as a CMake target or a documented command that runs after the macOS build.",
            "`tools/check_mac_bundle.sh` or a CMake custom target in `apps/desktop/CMakeLists.txt`.",
            "None. The bundle is already produced on macOS.",
            [
                "The check passes against `build/debug/apps/desktop/Debug/Vrowser.app`.",
                "The check fails when a helper name is wrong. A unit-style dry run against a fixture directory is enough.",
                "The README or `AGENTS.md` says how to run it.",
            ],
            "Run the check on the local debug bundle. No CEF test is required beyond that.",
            ["Notarization", "A new signing identity", "Disabling the sandbox"],
            DONE,
        ),
    },
    {
        "title": "Navigate the already open window",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:feature", "priority:p0", "area:core", "area:cef"],
        "body": body(
            "Startup `--url` loads the first page. Later commands need to load a different URL in the window that is already open, without including CEF headers in the command code.",
            "Add a small navigation type in `src/core` that stores the requested URL. The CEF client performs `LoadURL` on the UI thread when that request changes. Cover the core type with tests. Keep the CEF call inside `src/browser/cef`.",
            "`include/vrowser/core/`, `src/core/`, `src/browser/cef/cef_browser_client.cpp`, `tests/unit/`.",
            "Depends on the issue titled \"Compare Alloy style and Chrome style for the Vrowser shell\". The load path must match the chosen style.",
            [
                "A core test requests a URL and observes it without CEF.",
                "The open browser loads that URL on the CEF UI thread.",
                "A second launch with `--url` still reaches the running instance.",
            ],
            "Unit tests for the core type. Manually load a second https URL in the running macOS app and record the `vrowser: load-end` line.",
            ["A tab strip", "History persistence", "The command-line UI"],
            DONE,
        ),
    },
    {
        "title": "Create and close tabs",
        "milestone": "M1 — CEF Browser Prototype",
        "labels": ["type:feature", "priority:p0", "area:core", "area:cef", "area:ui"],
        "body": body(
            "A browser needs more than one page. The tab list has to exist before keyboard tab commands do.",
            "Track an ordered list of open browsers in core. Opening a tab creates one CEF browser. Closing the last tab quits. Closing another tab leaves the remaining one visible.",
            "`src/core/`, `src/browser/cef/cef_browser_client.cpp`, `tests/unit/`.",
            "Depends on the issue titled \"Navigate the already open window\".",
            [
                "Unit tests cover an empty list, append, close-middle, and close-last.",
                "Two pages can be open in one process on macOS.",
                "Closing the last page still reaches `CefShutdown`.",
            ],
            "Core unit tests. A manual macOS check that opens two URLs and closes one.",
            ["A drawn tab strip", "Keyboard shortcuts", "Session restore"],
            DONE,
        ),
    },
    {
        "title": "Add the Vim mode state machine",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim", "agent-ready"],
        "body": body(
            "Normal, Insert, Command, and Hint modes are the input model. The transitions can be tested before any key is read from the window.",
            "Implement a mode machine with an explicit transition function. Insert is selected when focus is in an editable element. Escape returns to Normal. Entering Command or Hint is a separate request. The machine does not parse key sequences.",
            "`src/vim/`, `include/vrowser/vim/`, `tests/unit/mode_machine_test.cpp`, `src/vim/CMakeLists.txt`.",
            "None. Do not include CEF headers.",
            [
                "Tests cover Normal, Insert, Command, and Hint.",
                "Escape from Insert, Command, and Hint returns to Normal.",
                "An editable-focus flag forces Insert, and clearing it returns to Normal only when the user did not choose another mode.",
            ],
            "`ctest --preset ci` includes the new tests.",
            ["Key parsing", "Bindings", "Painting a mode indicator"],
            DONE,
        ),
    },
    {
        "title": "Normalize keyboard events",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim"],
        "body": body(
            "Mode logic must see one key type on macOS, Linux, and Windows.",
            "Define a `KeyEvent` with key identity, produced character, and Control, Alt, Meta, and Shift. Add pure functions that map a small fixture of platform-shaped inputs into that struct. Do not subscribe to CEF yet.",
            "`src/vim/key_event.hpp`, `src/vim/key_event.cpp`, `tests/unit/key_event_test.cpp`.",
            "Depends on the issue titled \"Add the Vim mode state machine\".",
            [
                "Letters, Escape, Enter, and Tab have stable identities.",
                "Shift changes the character and sets the Shift modifier.",
                "The mapper has no CEF include.",
            ],
            "Unit tests for the mapper. No window test.",
            ["Reading events from CEF or the operating system", "Keybindings"],
            DONE,
        ),
    },
    {
        "title": "Add the keybinding registry",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim"],
        "body": body(
            "A key sequence has to resolve to a command name, not to a function buried in the UI.",
            "Register bindings per mode. Lookup returns the command name and arguments, or reports that the sequence is still a prefix.",
            "`src/vim/key_bindings.hpp`, `src/vim/key_bindings.cpp`, `tests/unit/key_bindings_test.cpp`.",
            "Depends on the issue titled \"Normalize keyboard events\".",
            [
                "A single-key binding resolves.",
                "A two-key binding reports a prefix after the first key.",
                "An unknown sequence clears the buffer.",
            ],
            "Unit tests only.",
            ["The default Vim scrolling commands", "User config files"],
            DONE,
        ),
    },
    {
        "title": "Parse counts and multi-key sequences",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim"],
        "body": body(
            "Commands such as `12j` and `gg` need a count and a prefix buffer.",
            "Extend the binding lookup so leading digits form a count. `0` is a command when it is the first key and a digit after a count has started. The result is a command name plus a count.",
            "`src/vim/key_bindings.cpp`, `tests/unit/key_bindings_test.cpp`.",
            "Depends on the issue titled \"Add the keybinding registry\".",
            [
                "`j` has count 1.",
                "`12j` has count 12.",
                "`gg` stays a prefix after the first `g` and resolves on the second.",
                "`0` as the first key does not start a count.",
            ],
            "Unit tests for counts and prefixes.",
            ["Executing scroll commands in a page", "Timeouts for abandoned prefixes"],
            DONE,
        ),
    },
    {
        "title": "Implement Normal and Insert modes in the window",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p0", "area:vim", "area:cef"],
        "body": body(
            "The state machine and the key parser do nothing until the browser consumes or forwards the key.",
            "In Normal mode, consume keys that the binding registry recognizes. In Insert mode, leave the key for the focused frame. Escape in Insert mode returns to Normal and is consumed.",
            "`src/vim/`, `src/browser/cef/` keyboard handler.",
            "Depends on the issues titled \"Parse counts and multi-key sequences\" and \"Create and close tabs\".",
            [
                "A focused text field starts in Insert mode.",
                "Escape leaves that field's Insert mode.",
                "A Normal-mode binding does not reach page JavaScript.",
            ],
            "Unit tests stay green. Manually verify a text field and a non-editable page on macOS.",
            ["Hint mode", "Command-line rendering", "Custom user keymaps"],
            DONE,
        ),
    },
    {
        "title": "Scroll the page from Normal mode",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim", "area:cef"],
        "body": body(
            "Basic Vim motion is the first command that must move a real page.",
            "Bind `j`, `k`, `d`, `u`, `g g`, and `G` to vertical scroll of the focused frame. Honor the numeric count.",
            "`src/vim/`, `src/commands/`, `src/browser/cef/`.",
            "Depends on the issue titled \"Implement Normal and Insert modes in the window\".",
            [
                "`j` and `k` move the page by one line step.",
                "A count repeats the step.",
                "`gg` and `G` reach the top and bottom of the focused frame.",
            ],
            "Unit-test the binding table. Manually scroll a long page on macOS.",
            ["Horizontal scroll", "Smooth scrolling settings", "Link hints"],
            DONE,
        ),
    },
    {
        "title": "Add the command registry and parser",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:core", "agent-ready"],
        "body": body(
            "Keybindings and the future command line need one place that names a command and runs it.",
            "Register commands by name with a one-line description. Parse a single input line into a name and arguments. Unknown names return an error. Do not open a window.",
            "`src/commands/`, `include/vrowser/commands/`, `tests/unit/command_registry_test.cpp`.",
            "None. Keep CEF headers out of this module.",
            [
                "A registered command runs with its arguments.",
                "An unknown command fails without throwing out of the parser.",
                "The description is available for later completion.",
            ],
            "`ctest --preset ci` includes the registry tests.",
            ["The on-screen command line", "Fuzzy search", "User-defined commands from a file"],
            DONE,
        ),
    },
    {
        "title": "Show command mode",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:ui", "area:vim"],
        "body": body(
            "Command mode needs a place to type. The registry already knows how to execute the line.",
            "Add a single-line input that appears when the mode machine enters Command. Enter executes the registry. Escape cancels and returns to Normal.",
            "`src/ui/`, `src/commands/`, the desktop window host.",
            "Depends on the issues titled \"Add the command registry and parser\" and \"Implement Normal and Insert modes in the window\".",
            [
                "`:open https://example.com` navigates the active tab once navigation exists.",
                "Escape closes the line and does not execute it.",
                "An unknown command is reported without crashing.",
            ],
            "Registry unit tests stay green. Manually run one command on macOS.",
            ["Fuzzy search", "Command history persistence"],
            DONE,
        ),
    },
    {
        "title": "Add link hints",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p1", "area:vim", "area:ui", "area:cef"],
        "body": body(
            "Follow-link is the other basic Vim browsing action.",
            "Hint mode labels visible links in the focused frame. Typing a unique label activates that link. Escape cancels. Hints disappear when the mode exits.",
            "`src/vim/`, `src/browser/cef/` for a short-lived frame query.",
            "Depends on the issue titled \"Implement Normal and Insert modes in the window\".",
            [
                "Hints appear only in Hint mode.",
                "Choosing a label navigates or activates the control.",
                "Escape removes the labels and returns to Normal.",
            ],
            "Unit-test label assignment for a list of rectangles. Manually hint a page with several links on macOS.",
            ["Hints inside cross-origin iframes", "A persistent content script"],
            DONE,
        ),
    },
    {
        "title": "Follow focus across editable elements and frames",
        "milestone": "M2 — Vim Navigation Engine",
        "labels": ["type:feature", "priority:p0", "area:vim", "area:cef", "area:security"],
        "body": body(
            "Insert mode is wrong if it ignores focus inside a frame or stays on after focus leaves a text field.",
            "Observe CEF focus notifications and update the mode machine. Treat editable elements and content-editable regions as Insert. Do not trust page script to set the mode except through those notifications.",
            "`src/browser/cef/`, `src/vim/`.",
            "Depends on the issue titled \"Implement Normal and Insert modes in the window\".",
            [
                "Focusing an input enters Insert.",
                "Blur returns to Normal.",
                "A child frame with an input is handled the same way.",
            ],
            "Mode-machine unit tests for the focus flag. Manual check of a page with an iframe input.",
            ["Changing the page's JavaScript focus model", "Hint mode"],
            DONE,
        ),
    },
    {
        "title": "Add the SQLite migration runner",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:storage"],
        "body": body(
            "History and bookmarks need a schema that can change. The runner has to exist before those tables do.",
            "Open a SQLite database file and apply ordered migrations once. Record the current version. A failing migration leaves the previous version in place.",
            "`src/storage/`, `tests/unit/migration_test.cpp`.",
            "Depends on the issue titled \"Create and close tabs\", so the database has a profile directory to live in.",
            [
                "A fresh file ends at the latest version.",
                "A second open does not reapply migrations.",
                "A migration that fails does not advance the version.",
            ],
            "Unit tests against a temporary database file.",
            ["History or bookmark tables", "A network sync of the database"],
            DONE,
        ),
    },
    {
        "title": "Record browsing history",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:storage", "area:cef"],
        "body": body(
            "Visited URLs should survive a restart.",
            "Write a row when a main-frame load commits: URL, title, and time. Read the newest rows back. Keep the SQL inside `src/storage`.",
            "`src/storage/`, `src/browser/cef/` load handler.",
            "Depends on the issue titled \"Add the SQLite migration runner\".",
            [
                "A committed main-frame navigation inserts one row.",
                "Subframe loads do not insert rows.",
                "Reopening the database returns the row.",
            ],
            "Storage unit tests. One manual navigation whose row is visible after restart.",
            ["Full-text search", "Sync"],
            DONE,
        ),
    },
    {
        "title": "Store bookmarks",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:storage"],
        "body": body(
            "The user needs a durable list of URLs that is separate from history.",
            "Add bookmark rows and commands to add, remove, and list them. Names go through the command registry once command mode exists.",
            "`src/storage/`, `src/commands/`.",
            "Depends on the issues titled \"Add the SQLite migration runner\" and \"Show command mode\".",
            [
                "Add, list, and remove work in unit tests.",
                "A bookmark is still present after the process restarts.",
            ],
            "Storage unit tests and one manual command on macOS.",
            ["Bookmark folders", "Import and export"],
            DONE,
        ),
    },
    {
        "title": "Separate profiles with CefRequestContext",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:cef", "area:storage", "area:security"],
        "body": body(
            "One `root_cache_path` can hold more than one profile. Cookies must not leak between them.",
            "Create a `CefRequestContext` per profile directory under the existing root. Select the profile at startup with a switch. Document that two processes still cannot write the same root.",
            "`src/browser/cef/`, `src/core/startup_config.cpp`.",
            "Depends on the issue titled \"Add the SQLite migration runner\".",
            [
                "Two profiles write under different child directories.",
                "A cookie set in one profile is not sent in the other.",
                "The sandbox stays enabled.",
            ],
            "Manual cookie check on macOS. A unit test for the profile path helper.",
            ["A profile manager window", "Syncing profiles"],
            DONE,
        ),
    },
    {
        "title": "Restore the previous session",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:storage", "area:cef"],
        "body": body(
            "Quitting should not discard the open tabs.",
            "Save the ordered tab URLs when the last window closes. On the next launch, reopen them unless the user passed `--url`.",
            "`src/storage/`, `src/core/`, `src/browser/cef/`.",
            "Depends on the issues titled \"Create and close tabs\" and \"Add the SQLite migration runner\".",
            [
                "Two open tabs are reopened in order.",
                "`--url` skips restore.",
                "A corrupt session file launches the default page.",
            ],
            "Unit tests for the session record. A manual quit and relaunch on macOS.",
            ["Restoring form data", "Crash recovery of in-memory editors"],
            DONE,
        ),
    },
    {
        "title": "Handle downloads",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:cef", "area:security"],
        "body": body(
            "A file download should land on disk without giving the page a path of its own choosing.",
            "Implement the CEF download handler. Save into a downloads directory inside the profile. Reject a suggested path that escapes that directory.",
            "`src/browser/cef/`.",
            "Depends on the issue titled \"Navigate the already open window\".",
            [
                "A download completes inside the profile downloads directory.",
                "A suggested name containing `..` does not escape that directory.",
                "The page does not choose an arbitrary filesystem path.",
            ],
            "A path-sanitizer unit test. One manual download on macOS.",
            ["An upload manager", "Opening the file automatically"],
            DONE,
        ),
    },
    {
        "title": "Record permission decisions",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:cef", "area:security"],
        "body": body(
            "Permission prompts must stay explicit. The shell must not allow geolocation, notifications, or similar capabilities because it is easier.",
            "Handle the CEF permission request callback. Default to deny. Persist an allow or deny per origin for the small set of permissions CEF reports. Show the decision through the current runtime style's UI path.",
            "`src/browser/cef/`, `src/storage/` if the decision is persisted.",
            "Depends on the issue titled \"Compare Alloy style and Chrome style for the Vrowser shell\".",
            [
                "An unseen permission is denied until the user allows it.",
                "A stored deny is applied on the next request.",
                "No code path allows every permission.",
            ],
            "Unit tests for the decision table. One manual prompt on macOS, with the style named in the pull request.",
            ["A custom permission panel", "Camera and microphone device selection beyond CEF's prompt"],
            DONE,
        ),
    },
    {
        "title": "Add a command palette",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p2", "area:ui", "area:core"],
        "body": body(
            "Command mode is exact. A palette lets the user search command names.",
            "List registered commands and filter them by a substring of the name or description. Choosing a row runs that command.",
            "`src/commands/`, `src/ui/`.",
            "Depends on the issue titled \"Show command mode\".",
            [
                "Filtering is covered by a unit test.",
                "Selecting a row executes the command.",
                "Escape closes the palette.",
            ],
            "Unit tests for the filter. A manual open of the palette on macOS.",
            ["Fuzzy scoring beyond substring match", "Plugin-provided icons"],
            DONE,
        ),
    },
    {
        "title": "Switch tabs from the keyboard",
        "milestone": "M3 — Browser Essentials",
        "labels": ["type:feature", "priority:p1", "area:vim", "area:ui"],
        "body": body(
            "Tabs are not usable from the keyboard until Normal mode can select them.",
            "Bind next-tab, previous-tab, and close-tab in the keybinding registry. The commands call the tab list.",
            "`src/vim/`, `src/core/`, `src/commands/`.",
            "Depends on the issues titled \"Create and close tabs\" and \"Implement Normal and Insert modes in the window\".",
            [
                "The bindings are unit-tested.",
                "Next and previous wrap.",
                "Close-tab on the last tab quits.",
            ],
            "Binding and tab-list unit tests. A manual check with two tabs on macOS.",
            ["A mouse tab strip", "Dragging tabs"],
            DONE,
        ),
    },
    {
        "title": "Write the plugin API RFC",
        "milestone": "M4 — Extensibility",
        "labels": ["type:docs", "priority:p1", "area:core", "area:security"],
        "body": body(
            "A plugin runtime should not start until the API and its trust boundary are written down.",
            "Draft an RFC that lists the commands a plugin may register, the data it may read, and the process it runs in. The RFC does not add code.",
            "`docs/rfcs/0002-plugins.md`.",
            "Depends on the issue titled \"Show command mode\".",
            [
                "The RFC names the allowed calls.",
                "The RFC states that page content is untrusted input to a plugin.",
                "The RFC says what is deferred.",
            ],
            "No code test. The review is the check.",
            ["An implementation", "Loading a sample plugin"],
            DONE,
        ),
    },
    {
        "title": "Specify sandboxed plugin execution",
        "milestone": "M4 — Extensibility",
        "labels": ["type:research", "priority:p1", "area:security"],
        "body": body(
            "Plugins that can see cookies or the filesystem need a sandbox story separate from the renderer sandbox.",
            "Compare an in-process API, a subprocess with no filesystem rights, and refusing native code. Recommend one. Do not implement it.",
            "`docs/architecture/plugins.md`.",
            "Depends on the issue titled \"Write the plugin API RFC\".",
            [
                "The note names one execution model.",
                "The note says which Chromium sandbox it reuses, if any.",
                "No plugin loader is added.",
            ],
            "No runtime test.",
            ["Shipping a plugin loader", "npm or a JavaScript runtime inside the shell"],
            DONE,
        ),
    },
    {
        "title": "Load user configuration",
        "milestone": "M4 — Extensibility",
        "labels": ["type:feature", "priority:p1", "area:core"],
        "body": body(
            "Keymaps and settings need a file the user can edit.",
            "Read one config file from the profile directory. Apply command and keymap entries through the existing registries. A missing file keeps the defaults.",
            "`src/config/`, `src/vim/`, `src/commands/`.",
            "Depends on the issues titled \"Add the keybinding registry\" and \"Add the command registry and parser\".",
            [
                "A missing file does not change defaults.",
                "A keymap entry overrides one default binding in a unit test.",
                "An invalid file reports an error and keeps the previous bindings.",
            ],
            "Unit tests with temporary config files.",
            ["A GUI settings page", "Remote configuration"],
            DONE,
        ),
    },
    {
        "title": "Add a local CLI",
        "milestone": "M4 — Extensibility",
        "labels": ["type:feature", "priority:p2", "area:core"],
        "body": body(
            "A second process should be able to ask the running browser to run a command.",
            "Accept a command line on a local socket or equivalent loopback channel. Forward it to the command registry in the browser process. Refuse connections that are not from the local user.",
            "`src/commands/`, `apps/desktop/`.",
            "Depends on the issue titled \"Show command mode\".",
            [
                "A local client runs one registered command.",
                "A non-local peer is rejected.",
                "The protocol is documented in `docs/architecture/`.",
            ],
            "A unit or integration test of the permission check. A manual local invocation.",
            ["A remote network API", "Authentication beyond the local user"],
            DONE,
        ),
    },
    {
        "title": "Allow custom commands and keymaps",
        "milestone": "M4 — Extensibility",
        "labels": ["type:feature", "priority:p2", "area:vim", "area:core"],
        "body": body(
            "After configuration loads, the user needs to add a command and bind a key to it.",
            "Support a config entry that names a new command as a sequence of existing commands, and a keymap entry that points at it.",
            "`src/config/`, `src/commands/`, `src/vim/`.",
            "Depends on the issue titled \"Load user configuration\".",
            [
                "A custom command runs its sequence.",
                "A keymap invokes that command from Normal mode in a unit test.",
                "A custom command cannot name an unknown command.",
            ],
            "Unit tests for the config entries.",
            ["Arbitrary native code in the config file", "A plugin runtime"],
            DONE,
        ),
    },
    {
        "title": "Record Chrome extension compatibility limits",
        "milestone": "M4 — Extensibility",
        "labels": ["type:research", "priority:p2", "area:cef", "area:docs"],
        "body": body(
            "CEF's Chrome runtime can load some Chrome extensions. Vrowser should say what works instead of implying full extension support.",
            "Try one unpacked extension that uses a documented Chrome API, and one that needs an API CEF does not implement. Write the limits.",
            "`docs/architecture/extensions.md`.",
            "Depends on the issue titled \"Compare Alloy style and Chrome style for the Vrowser shell\". Extensions require the Chrome runtime style.",
            [
                "The note names the CEF version and the two extensions.",
                "The note lists APIs that failed.",
                "The README does not claim Chrome Web Store compatibility.",
            ],
            "Manual load of the two extensions. No automated extension test.",
            ["A built-in extension store", "Rewriting an extension to fit Vrowser"],
            DONE,
        ),
    },
]


def run(args: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(args, check=False, text=True, capture_output=True)
    if check and result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise subprocess.CalledProcessError(
            result.returncode, args, output=result.stdout, stderr=detail
        )
    return result


def existing_labels(repo: str) -> set[str]:
    result = run(["gh", "label", "list", "--repo", repo, "--limit", "200", "--json", "name"])
    return {item["name"] for item in json.loads(result.stdout or "[]")}


def existing_milestones(repo: str) -> set[str]:
    result = run(
        [
            "gh",
            "api",
            f"repos/{repo}/milestones?state=all&per_page=100",
            "--jq",
            ".[].title",
        ]
    )
    return {line for line in result.stdout.splitlines() if line}


def existing_issues(repo: str) -> dict[str, dict[str, object]]:
    result = run(
        [
            "gh",
            "issue",
            "list",
            "--repo",
            repo,
            "--state",
            "all",
            "--limit",
            "200",
            "--json",
            "number,title,state",
        ]
    )
    found = {}
    for item in json.loads(result.stdout or "[]"):
        found[item["title"]] = item
    return found


def ensure_labels(repo: str) -> None:
    present = existing_labels(repo)
    for name, color, description in LABELS:
        if name in present:
            print(f"label exists: {name}")
            continue
        run(
            [
                "gh",
                "label",
                "create",
                name,
                "--repo",
                repo,
                "--color",
                color,
                "--description",
                description,
            ]
        )
        print(f"label created: {name}")


def ensure_milestones(repo: str) -> None:
    present = existing_milestones(repo)
    for title, description in MILESTONES:
        if title in present:
            print(f"milestone exists: {title}")
            continue
        run(
            [
                "gh",
                "api",
                f"repos/{repo}/milestones",
                "-f",
                f"title={title}",
                "-f",
                f"description={description}",
            ]
        )
        print(f"milestone created: {title}")


def create_issue(repo: str, issue: dict[str, object]) -> str:
    command = [
        "gh",
        "issue",
        "create",
        "--repo",
        repo,
        "--title",
        str(issue["title"]),
        "--body",
        str(issue["body"]),
        "--milestone",
        str(issue["milestone"]),
    ]
    for label in issue["labels"]:
        command.extend(["--label", str(label)])
    result = run(command)
    url = result.stdout.strip()
    print(f"issue created: {url}")
    return url


def close_issue(repo: str, number: int) -> None:
    run(
        [
            "gh",
            "issue",
            "close",
            str(number),
            "--repo",
            repo,
            "--reason",
            "completed",
            "--comment",
            f"Implemented on `main` in `{DONE_COMMIT}`.",
        ]
    )
    print(f"issue closed: #{number}")


def ensure_issues(repo: str) -> None:
    present = existing_issues(repo)
    for issue in CLOSED:
        title = str(issue["title"])
        current = present.get(title)
        if current is None:
            url = create_issue(repo, issue)
            number = int(url.rstrip("/").split("/")[-1])
            close_issue(repo, number)
            continue
        print(f"issue exists: #{current['number']} {title}")
        if current["state"] == "OPEN":
            close_issue(repo, int(current["number"]))
    present = existing_issues(repo)
    for issue in OPEN:
        title = str(issue["title"])
        if title in present:
            print(f"issue exists: #{present[title]['number']} {title}")
            continue
        create_issue(repo, issue)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default="iamcaye/vrowser")
    args = parser.parse_args()
    ensure_labels(args.repo)
    ensure_milestones(args.repo)
    ensure_issues(args.repo)
    return 0


if __name__ == "__main__":
    sys.exit(main())
