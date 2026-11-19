# Security model

Vrowser displays untrusted web content. The security boundary is Chromium's
multi-process sandbox, plus a shell that does not grant the page extra power.

## Trust boundary

Trusted: the browser process, the user-data directory, and local configuration
files the user edited.

Untrusted: HTML, scripts, network responses, downloaded files, renderer IPC,
and any string that arrived from a page. URL arguments on the command line are
trusted only as navigation targets. They are not passed to a shell.

The renderer is not allowed to call back into `src/core` or the filesystem.
When a render-process handler is added, it talks to the browser process through
CEF's process messages or a message router, and the browser process checks the
request.

## Sandbox

The CEF CMake option `USE_SANDBOX` stays ON. `CefSettings.no_sandbox` is set
only in a build where CEF did not define `CEF_USE_SANDBOX`. Release builds do
not define a flag that turns the sandbox off.

On macOS the sandbox also depends on:

- loading the CEF framework at runtime instead of linking `libcef`
- helper bundles with the identifiers and `LSUIElement` plist CEF expects
- the code signature and entitlements applied by CEF's framework copy step
- `MallocNanoZone=0` in the app and helper plists, which CEF requires for its
  allocator

Breaking that layout makes the helper fail closed. The fix is to repair the
bundle, not to disable the sandbox.

## Profile isolation

`root_cache_path` is unique to Vrowser so the singleton lock and the on-disk
cache are not shared with another CEF application. A second process that points
at the same directory does not open a second writer. CEF forwards the launch
to the running instance.

Cookies, cache, and local storage live under that directory. A future profile
feature will use separate `CefRequestContext` directories under the same root.
Profiles must not be allowed to read each other's cookie files.

## Web platform

The shell does not disable web security, site isolation, or certificate
checks. Certificate errors use Chromium's UI in Chrome style. Alloy style will
need an explicit decision before it is the default, because Alloy does not
automatically present the same permission and certificate UI. That is one
reason Chrome style is the prototype default.

Custom schemes, if they are added, are registered as standard schemes without
the privileges of `https`, and they are not allowed to fetch `file:` URLs.

Downloads and permission prompts are not implemented. Until they are, Chromium's
Chrome-style UI is the only handler. Do not auto-accept permission requests.

## Supply chain

CEF binaries are pinned by version and SHA256 in `cmake/cef_version.cmake`.
The download fails when the hash does not match. GoogleTest is fetched by its
v1.16.0 tag. Neither tree is vendored into git.

A packaged build must include CEF's `LICENSE.txt` and `CREDITS.html`. See
`THIRD_PARTY_NOTICES.md`.

## Reporting

See `SECURITY.md`. Do not file sandbox-escape details in a public issue before
the maintainers have acknowledged them.
