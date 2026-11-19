# Current status

Updated: 2026-11-19

## Milestone

M0 (project foundation) and the first slice of M1 (one CEF window that loads
a page) are implemented locally. Vim input, tabs, SQLite, and plugins are not
started.

## Implemented

- CMake presets `ci`, `debug`, and `release`. `ci` builds unit tests without
  downloading CEF.
- C++20 library `vrowser_core` with startup-switch parsing.
- GoogleTest tests for those switches.
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
| `cmake --preset ci && cmake --build --preset ci && ctest --preset ci` | 9 tests passed |
| `ctest --test-dir build/debug` | 9 tests passed |
| `cmake --preset debug && cmake --build --preset debug` | Built `Vrowser.app` and five helper bundles |
| CEF macOS arm64 archive SHA256 | `22a510e5d348a907953278b618e1e9e442ccf78773b9fee0e09eaf154eef53b0` |
| Smoke test, sandbox left on | `https://example.com` main frame HTTP 200, process exit 0 |
| Chrome-style smoke (`--vrowser-smoke-test`) | exit 0, `vrowser-smoke: ok 200` |
| Alloy-style smoke (`--use-alloy-style --vrowser-smoke-test`) | exit 0, `vrowser-smoke: ok 200` |
| `https://expired.badssl.com/` Chrome style | Chromium keeps the error UI; no client data URL |
| `https://expired.badssl.com/` Alloy style | Client data-URL error page via `OnLoadError` |
| `open build/debug/apps/desktop/Debug/Vrowser.app --args --url=https://example.com` | Launched |
| CEF Linux workflow (`cef-linux.yml` on `ubuntu-latest`) | Built `vrowser` and `chrome-sandbox`; sandbox left ON |

The passing smoke command was `tools/smoke_test.sh` after the macOS keychain
permission was available. It did not pass `--use-mock-keychain`.

Windows was not configured or run. Its entry points are in `apps/desktop`
and follow the current cefsimple structure. Linux CEF CI
(`.github/workflows/cef-linux.yml`) builds `vrowser` on `ubuntu-latest` with
the pinned `linux64` archive and `USE_SANDBOX=ON`. Graphical Linux smoke was
not run.

## Known limitations

- CEF logs `code_sign_clone_manager.mm` failing to clear a quarantine
  attribute on a temporary directory. The page load still completed.
- During shutdown CEF can log `waitpid: No child processes` and a GPU
  shared-image error. The process still returned 0 on the verified run.
- Chrome style shows Chromium's toolbar. The Vim shell should move to Alloy
  style after the UI gaps listed in
  [runtime-style.md](../architecture/runtime-style.md) are closed.
- No tabs, commands, history, or bookmarks.
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

Windows CEF CI (#10), the macOS bundle layout check (#11), the Vim mode
machine (#14), and the command registry (#20) are unblocked. Navigation in
the open window (#12) is unblocked after the runtime-style decision but still
needs the `agent-ready` label. Permission handling should follow Alloy's
client-owned prompt path.

## Recommended next task

[Build and smoke-test the desktop shell on Windows](https://github.com/iamcaye/vrowser/issues/10)
(`agent-ready`, `priority:p1`). On macOS-only machines,
[Check the macOS app bundle layout](https://github.com/iamcaye/vrowser/issues/11)
is the smallest local follow-up.
