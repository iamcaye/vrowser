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
  Windows. The workflow is in the repository. It had not completed a run
  when this status was written.
- Pinned CEF 156.0.30+g6028375+chromium-156.0.8078.96, Extended channel.
  SHA256 hashes for the official platform archives are in
  `cmake/cef_version.cmake`.
- Desktop shell: Chrome bootstrap, CEF Views, default Chrome runtime style,
  `--use-alloy-style`, `--use-native`, profile directory, macOS helper
  bundles, clean shutdown through `CefQuitMessageLoop`.
- `tools/smoke_test.sh` loads a URL and exits.

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
| `open build/debug/apps/desktop/Debug/Vrowser.app --args --url=https://example.com` | Launched |

The passing smoke command was `tools/smoke_test.sh` after the macOS keychain
permission was available. It did not pass `--use-mock-keychain`.

Linux and Windows were not configured or run. Their entry points are in
`apps/desktop` and follow the current cefsimple structure.

## Known limitations

- CEF logs `code_sign_clone_manager.mm` failing to clear a quarantine
  attribute on a temporary directory. The page load still completed.
- During shutdown CEF can log `waitpid: No child processes` and a GPU
  shared-image error. The process still returned 0 on the verified run.
- Chrome style shows Chromium's toolbar. A Vim-style shell still needs a
  decision on Alloy style. See RFC 0001.
- No tabs, commands, history, or bookmarks.
- The macOS helpers are ad-hoc signed by the linker. A release package still
  needs a real signing and notarization pass.
- GoogleTest is fetched as the v1.16.0 tag. The desktop shell is pinned by
  SHA256. The test archive is not.

## Decisions

Recorded in [docs/rfcs/0001-architecture.md](../rfcs/0001-architecture.md).
CEF 156 was selected because it is the current Extended branch and this
machine matches its Xcode 27 / macOS 26.6 build requirement.

## Next unblocked work

The recommended next issue is the Alloy-style versus Chrome-style comparison.
Do that before building a custom toolbar or command line. Linux and Windows
CEF builds can proceed in parallel if they do not change the macOS bundle.

## Recommended next task

Compare Alloy style and Chrome style on macOS with the existing switches,
write the result under `docs/architecture/`, and recommend which style the
Vim shell should adopt.
