# Runtime style: Chrome versus Alloy

Updated: 2026-11-19

CEF 156 uses one process bootstrap (Chrome). Each window chooses a runtime
style at creation time. Vrowser defaults to Chrome style. `--use-alloy-style`
selects Alloy style. Both use CEF Views unless `--use-native` is set.

This note records a macOS comparison against the built `Vrowser.app` and the
recommendation for the future Vim shell. Linux and Windows were not run.

## Commands used

Host: macOS arm64, sandbox left on, CEF 156.0.30.

```bash
APP=build/debug/apps/desktop/Debug/Vrowser.app/Contents/MacOS/Vrowser

# Smoke: both styles load https://example.com and exit 0
$APP --vrowser-smoke-test --url=https://example.com \
  --user-data-dir=build/smoke-chrome
$APP --vrowser-smoke-test --use-alloy-style --url=https://example.com \
  --user-data-dir=build/smoke-alloy

# Interactive default and Alloy launches
$APP --url=https://example.com --user-data-dir=build/compare-chrome-ui
$APP --use-alloy-style --url=https://example.com \
  --user-data-dir=build/compare-alloy-ui

# Certificate failure
$APP --url=https://expired.badssl.com/ --user-data-dir=build/compare-chrome-cert
$APP --use-alloy-style --url=https://expired.badssl.com/ \
  --user-data-dir=build/compare-alloy-cert
```

Smoke results: Chrome exit 0 (`vrowser-smoke: ok 200`), Alloy exit 0
(`vrowser-smoke: ok 200`). Shutdown still logged the known
`code_sign_clone_manager` quarantine noise and occasional `waitpid: No child
processes` / GPU shared-image errors. Neither blocked exit 0.

## What differs

| Area | Chrome style | Alloy style |
| --- | --- | --- |
| Window chrome | Chromium toolbar, omnibox, and related Chrome UI | Client owns the chrome. No Chromium toolbar |
| Load failure / cert interstitial | Chromium error UI. Client `OnLoadError` returns early | Client must supply UI. Prototype loads a data-URL page |
| Permission prompts | Default CEF handling shows Chrome permission UI | Default handling denies media access and ignores prompts |
| Downloads | Default Chromium download UI | Default cancels unless the client implements a handler |
| Find-in-page | Chrome find UI / standard Chrome accelerators | No find bar. Use `CefBrowserHost::Find` plus client UI |
| Popups | Views path creates a top-level window via the shared delegate; Chrome also has Chrome-UI popup paths for native hosts | Same Views popup delegate; native Alloy creates a native popup |
| Fullscreen | Handled inside Chrome style | Client must drive the Views/native fullscreen transition |
| Windowless rendering | Unavailable | Required style for off-screen rendering |
| Extensions / Chrome commands | Chrome-style APIs (`CefCommandHandler`, Chrome toolbar view) | Not available |
| Vim command line / status bar | Fights Chromium's toolbar layout | Fits a client-drawn shell |
| Several browser views in one window (tabs) | Second browser view is never created (observed, macOS) | Works; `--vrowser-tab-smoke-test` passes |

### Observed certificate path

`https://expired.badssl.com/` with the sandbox on:

- Chrome: `load-end 0 https://expired.badssl.com/`. No client data URL. Chromium
  keeps the interstitial / error presentation.
- Alloy: `load-end 0`, then the client `OnLoadError` path loads a data URL whose
  body is `Failed to load URL https://expired.badssl.com/` with
  `ERR_CERT_DATE_INVALID (-201)`. That matches
  `CefBrowserClient::OnLoadError` in `src/browser/cef/cef_browser_client.cpp`.

### API sources for defaults Vrowser does not override yet

From the pinned CEF 156 headers:

- `CefPermissionHandler`: Chrome shows UI; Alloy denies media and ignores
  prompts by default.
- `CefClient` / `CefDownloadHandler`: Chrome proceeds with download UI; Alloy
  cancels.
- `CefBrowserView::GetChromeToolbar`: Chrome style only.
- `CefCommandHandler`: Chrome style only.
- `cef_types_runtime.h`: Alloy is the content-layer style; Chrome is the Chrome
  UI layer. Windowless rendering is Alloy-only.

## Recommendation

**Keep Chrome style as the prototype default.** Do not flip
`--use-alloy-style` on by default in this change. Chrome style is still the
path that loads a trustworthy page with Chromium's toolbar, permission
prompts, certificate UI, and download UI without Vrowser owning those
surfaces yet.

**Plan the Vim shell on Alloy style.** A modal command line, status bar, and
hint overlay need a window Chromium does not already fill with a toolbar.
Alloy matches that ownership model. Adopt Alloy as the product default only
after Vrowser implements:

1. Permission prompts (deny by default, persist allow/deny per origin).
2. Certificate / load-error UI that is clearer than the prototype data URL.
3. Download handling.
4. Find-in-page (or an explicit decision that `/` search replaces it).
5. A check that Views popups and clean shutdown still match the smoke test.

Until those land, develop the command line and status bar against
`--use-alloy-style` while leaving the shipped default on Chrome style so
day-to-day browsing remains usable.

## Out of scope here

- Building a custom toolbar or status bar.
- Keybindings.
- Linux and Windows verification of the same matrix.
- Changing `src/browser/cef/cef_runtime_app.cpp` defaults.

## References

- [RFC 0001](../rfcs/0001-architecture.md)
- [Process model](process-model.md)
- [Security model](security-model.md)
- https://chromiumembedded.github.io/cef/architecture.html
- `include/internal/cef_types_runtime.h` in the pinned CEF distribution
