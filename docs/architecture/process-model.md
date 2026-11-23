# Process model

CEF 156 uses the Chrome bootstrap only. The Alloy bootstrap was removed in
M128. Runtime style is chosen per window.

References:

- https://chromiumembedded.github.io/cef/architecture.html
- https://chromiumembedded.github.io/cef/tutorial.html
- `include/internal/cef_types_runtime.h` inside the pinned binary distribution

## Processes

| Process | macOS host | Linux and Windows host | Role |
| --- | --- | --- | --- |
| Browser | `Vrowser.app` `Contents/MacOS/Vrowser` | `vrowser` | UI thread, network, window, `CefApp` |
| Renderer | `Vrowser Helper (Renderer).app` | same executable, `--type=renderer` | Blink, isolated |
| GPU | `Vrowser Helper (GPU).app` | same executable | compositing |
| Plugin and alerts | matching helper bundles | same executable | utility processes |

The macOS helpers are produced by the loop over `CEF_HELPER_APP_SUFFIXES` in
`apps/desktop/CMakeLists.txt`. Each helper calls `CefExecuteProcess` after
`CefScopedLibraryLoader::LoadInHelper()`. When the sandbox is enabled, the
helper first initializes `CefScopedSandboxContext`.

The browser process loads the CEF framework with `LoadInMain()` and does not
link `libcef` directly. That is a sandbox requirement from CEF's macOS
packaging, not an extra abstraction.

`CefInitialize` runs the browser process. `CefRunMessageLoop` blocks until
`CefQuitMessageLoop`, which the client calls after the last browser closes.
`CefShutdown` runs only after the message loop returns.

## Runtime style

From the CEF 156 header `cef_types_runtime.h`:

- Chrome style uses the Chrome UI layer. It provides Chromium's toolbar,
  permission prompts, and related dialogs. Windowless rendering is unavailable.
- Alloy style uses the Chromium content layer. The client owns more of the
  window. Windowless rendering uses Alloy style.
- A Chrome-style `CefWindow` can host one Chrome-style `CefBrowserView` and
  additional Alloy-style views. An Alloy-style window hosts Alloy views only.
- The style is fixed when the window and browser view are created. Passing
  `CEF_RUNTIME_STYLE_DEFAULT` on a windowed Views browser resolves to Chrome
  style. The client checks the resolved style in `OnAfterCreated`.

The prototype default is Views plus `CEF_RUNTIME_STYLE_DEFAULT`. `--use-alloy-style`
passes `CEF_RUNTIME_STYLE_ALLOY` to both the window delegate and the browser
view delegate. `--use-native` sets `CefWindowInfo.runtime_style` and calls
`CefBrowserHost::CreateBrowser`.

Chrome style and Alloy style do not share every callback. Alloy load failures
are rendered by the client as a data URL. Chrome style keeps Chromium's error
page. Title changes for a Views window go through `CefWindow::SetTitle`. Native
Alloy windows use a small platform function. Those platform functions are the
only AppKit, Win32, and X11 calls outside process startup. The full comparison
and the Vim-shell recommendation are in [runtime-style.md](runtime-style.md).

## Threads

Browser-process UI thread: `OnContextInitialized`, life-span callbacks, load
callbacks, and window delegate methods. The client posts relaunch, show, and
close work back to `TID_UI` when it is invoked from elsewhere.

`OnAlreadyRunningAppRelaunch` is documented to run on the UI thread. The
implementation still checks, because a future caller may not.

Renderer and GPU code is not customized. The helper passes a null `CefApp`.
That is enough to load pages. Message routers and custom render handlers are
future work and will require a real `CefApp` in the helper.

## Profile directory

`ConfigureCefSettings` sets three absolute paths under the user-data directory:

- `root_cache_path`: the user-data directory itself. CEF 120 and newer use it
  as the process singleton lock.
- `cache_path`: the selected profile's directory (below).
- `log_file`: `vrowser.log` in the user-data directory. Severity is warning.

`--profile=<name>` selects a profile; the default is `default`. A name is 1
to 64 of `a`-`z`, `0`-`9`, `_`, and `-`, and does not start with `-`. Any
other name stops startup with status 1. Chromium only creates profiles that
are direct children of the user-data directory, so:

| Profile | Directory |
| --- | --- |
| `default` | `<user-data-dir>/Default` (Chromium's own name) |
| any other `<name>` | `<user-data-dir>/Profile-<name>` |

After CEF starts, the browser process creates one `CefRequestContext` with
that directory as its `cache_path`. That context initializes asynchronously,
and the first window opens in its `OnRequestContextInitialized`. Every tab
is created with the same context, so cookies, cache, and local storage stay
in that directory. Vrowser's `vrowser.db` lives there too. `vrowserrc` stays
in the user-data directory and is shared by all profiles.

Two processes cannot use one user-data directory at the same time, whatever
their profiles. A second launch with the same `--user-data-dir` is forwarded
to the running process (`OnAlreadyRunningAppRelaunch`), which opens the URL
in its own profile. To run two profiles at once, give each process its own
`--user-data-dir`.

Default directory:

| Platform | Path |
| --- | --- |
| macOS | `~/Library/Application Support/Vrowser` |
| Linux | `${XDG_DATA_HOME:-~/.local/share}/vrowser` |
| Windows | `%LOCALAPPDATA%\Vrowser` |

`--user-data-dir` overrides it. The directory is created before
`CefInitialize`. A failure there exits the process with status 1 and does not
call CEF.

## Shutdown

Closing the last window calls `TryCloseBrowser` from the Views delegate.
`DoClose` marks the final browser as closing and returns false so the OS close
proceeds. `OnBeforeClose` quits the message loop once the browser list is
empty. The macOS `terminate:` implementation asks the client to close browsers
and returns, so `exit()` does not skip `CefShutdown`.

## Sandbox

`USE_SANDBOX` defaults to ON in the CEF CMake package. The app sets
`CefSettings.no_sandbox` only when `CEF_USE_SANDBOX` is absent. Packagers should
keep the helpers, the framework, and the entitlements that CEF's
`COPY_MAC_FRAMEWORK` macro applies. Linux uses `SET_LINUX_SUID_PERMISSIONS` on
`chrome-sandbox`. Windows sandbox builds are a DLL launched by CEF's
`bootstrap.exe`, with LPAC ACLs applied by `SET_LPAC_ACLS`.
