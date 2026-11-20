# Vrowser

Vrowser is an open-source, keyboard-first web browser. The shell is C++20.
The web engine is the official Chromium Embedded Framework.

The current milestone is a foundation and a single-window prototype. It
initializes CEF, opens a native window, loads a page, and shuts down. Vim
modes, tabs, and storage are specified and not built yet. Agents should start
at [AGENTS.md](AGENTS.md) and [docs/progress/current-status.md](docs/progress/current-status.md).

## License

Vrowser is [Apache-2.0](LICENSE). CEF and Chromium keep their own licenses.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Build

Requirements: CMake 3.24 or newer, Ninja, and a C++20 compiler. The pinned CEF
build for this tree is 156.0.30 (Chromium 156.0.8078.96). On macOS that build
expects Xcode 27. The first configure downloads CEF into `third_party/cef/`
and checks the SHA256 in `cmake/cef_version.cmake`.

[`just`](https://github.com/casey/just) wraps the commands below. Run `just`
to list recipes. `just test` runs the unit tests, `just run [url]` builds and
launches the macOS app with a profile in `build/dev-profile`, and `just smoke`
runs the smoke check. These recipes pass `--use-mock-keychain` so macOS does
not ask for the "Chromium Safe Storage" keychain item after every rebuild.
Launching the app another way still uses the login keychain. The raw CMake commands still work without `just`.

Unit tests do not need CEF:

```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
```

Desktop shell:

```bash
cmake --preset debug
cmake --build --preset debug
```

On macOS the app is `build/debug/apps/desktop/Debug/Vrowser.app`.

```bash
open build/debug/apps/desktop/Debug/Vrowser.app
```

```bash
build/debug/apps/desktop/Debug/Vrowser.app/Contents/MacOS/Vrowser \
  --url=https://example.com
```

`tools/smoke_test.sh` loads `https://example.com`, expects HTTP 200, and
waits for the process to exit.

`tools/check_mac_bundle.sh` verifies the macOS app contains the main
executable, the five helper bundles, and
`Chromium Embedded Framework.framework`. After a debug build you can also run
`cmake --build --preset debug --target check_mac_bundle`.

Switches:

| Switch | Effect |
| --- | --- |
| `--url=<url>` | First page. Default `https://example.com`. |
| `--user-data-dir=<path>` | Profile directory. |
| `--use-alloy-style` | Alloy window instead of the default Chrome style. |
| `--use-native` | Platform window instead of CEF Views. |
| `--vrowser-smoke-test` | Quit after the main frame reports a successful status. |
| `--vrowser-key-smoke-test` | Load a built-in test page, send keys, check Normal-mode bindings are consumed and a focused field selects Insert, and quit. Chrome style only. |
| `--vrowser-tab-smoke-test` | Open the URL in a second tab, close the first, check the second is shown, and quit. Needs `--use-alloy-style`. |

## Layout

```text
apps/desktop     process entry and packaging
src/core         startup configuration
src/browser/cef  CEF adapter
include/vrowser  public headers
tests/unit       GoogleTest
docs             architecture, RFCs, status
```

## Development

Human contributors: [CONTRIBUTING.md](CONTRIBUTING.md).
Security reports: [SECURITY.md](SECURITY.md).
Architecture: [docs/architecture/overview.md](docs/architecture/overview.md)
and [docs/rfcs/0001-architecture.md](docs/rfcs/0001-architecture.md).
