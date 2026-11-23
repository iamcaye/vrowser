# RFC 0002 — Plugin API

- Status: proposed
- Date: 2026-11-23
- Issue: #33. The execution model and its sandbox are #34
  (`docs/architecture/plugins.md`).

## Problem

Vim and Neovim are useful because users extend them. Vrowser should be
extensible too, but a browser plugin sits next to cookies, history, and every
page the user opens. A runtime must not start before the API and its trust
boundary are written down. This RFC lists what a plugin may call, what it may
read, where it runs, and what is deferred. It adds no code.

## Goals

- Plugins add commands and key mappings that behave like built-in ones: they
  run through the command registry, take a count, and can be remapped in
  `vrowserrc`.
- A plugin gets only the browsing data the user granted it.
- A broken or hostile plugin cannot read the profile directory, run code in a
  page, or stall the browser.

## Non-goals

- Chrome extension compatibility. That is a separate question (#38).
- Scripts that run inside web pages (user scripts, content scripts).
- A plugin marketplace, signing, or automatic updates.

## Trust model

There are three levels, from most to least trusted:

1. **The browser process and the user's own configuration.** That means
   `vrowserrc` and the profile directory.
2. **Plugins.** The user chose to install a plugin, but its code is not
   reviewed by Vrowser. It is treated as untrusted by the browser process, the
   same way renderer IPC is. Every call is checked against the plugin's
   granted capabilities, and every argument is validated as if a page had sent
   it.
3. **Page content.** Anything that originated in a page is untrusted input to
   the plugin as well as to the shell. That covers URLs, titles, text the page
   reports about itself, download names, and redirects. The shell hands it to
   a plugin as data fields, never as code or as a command line to run. A
   plugin that forwards page text back into a call gets the same validation
   as any other argument.

Configuration in `vrowserrc` wins over plugins. A plugin cannot unmap or
replace a mapping or a command the user defined.

## Where a plugin runs

A plugin never runs in a renderer, and never in the browser process.

Each plugin runs in its own **plugin host process**, started by the browser
process. It talks to the browser process only through one message channel
that the browser process owns. The host has no filesystem rights beyond its
own package (read-only), no network access, and no handle to CEF, SQLite, or
the profile directory. Everything it can do is one of the calls below.

The language runtime inside the host, and which operating-system or
Chromium sandbox confines it, are decided by #34. This RFC fixes only the
boundary: one process per plugin, messages only, and no ambient authority.
The CEF threading rules do not reach plugins. The browser process receives
messages on the UI thread and answers asynchronously, so a plugin cannot block
the UI thread.

## Manifest and capabilities

A plugin is a directory with a manifest:

```text
name         lowercase letters, digits, and `-`; unique among installed plugins
version      semver
entry        the file the host loads
capabilities the list below; anything not listed is refused
```

The user grants the capabilities when installing the plugin. A grant is
stored per plugin in the profile, and the user can revoke it. A call that
needs a capability the plugin was not granted fails with an error. The shell
does not prompt for it at that moment.

| Capability | Grants |
| --- | --- |
| *(none)* | `commands.register`, `status.show`, `storage.*`, and the events for the plugin's own commands |
| `commands.run` | Running registered commands by name |
| `keys` | `keys.map` |
| `tabs.read` | `tabs.list` and the tab events |
| `navigate` | `navigation.open` |
| `history.read` | `history.query` |
| `bookmarks.read` | `bookmarks.list` |
| `bookmarks.write` | `bookmarks.add`, `bookmarks.remove` |

## Allowed calls

These are the only calls in version 1. Anything not in this table does not
exist. All calls are asynchronous and return a result or an error string.

### Plugin to browser

| Call | Capability | Behavior |
| --- | --- | --- |
| `commands.register(name, description)` | none | Adds `<plugin>.<name>` to the command registry. The registry's name rules apply. The plugin can only register under its own prefix. |
| `commands.run(name, args, count)` | `commands.run` | Runs a registered command, the same way a key binding does. The call depth is limited so plugins cannot loop through each other. |
| `keys.map(keys, command)` | `keys` | Adds a Normal-mode mapping, unless `vrowserrc` already maps those keys. Other modes are deferred. |
| `status.show(text)` | none | Shows one line in the status bar, prefixed with the plugin name. Control characters are removed and the text is cut to the status bar. |
| `tabs.list()` | `tabs.read` | Returns each tab's ID, its committed URL as the browser process knows it, its title, and whether it is active. The title comes from the page. |
| `navigation.open(text, where)` | `navigate` | Loads text in the current tab or a new tab. It applies the same rules as `:open`: `javascript:`, `data:`, `file:`, and internal schemes are rejected. |
| `history.query(text, limit)` | `history.read` | Returns matching visits (URL, title, time), newest first, with a capped limit. |
| `bookmarks.list()` | `bookmarks.read` | Returns all bookmarks. |
| `bookmarks.add(url, title)`, `bookmarks.remove(url)` | `bookmarks.write` | Same rules as `:bookmark-add` and `:bookmark-remove` (#26): `http` and `https` only. |
| `storage.get(key)`, `storage.set(key, value)`, `storage.delete(key)` | none | A key-value store private to the plugin, in the profile database, with a size quota. |

### Browser to plugin

| Event | Capability | When |
| --- | --- | --- |
| `command.invoked(name, args, count, tab)` | none | One of the plugin's commands ran. |
| `tab.committed(tab, url)` | `tabs.read` | A main-frame navigation committed. |
| `tab.closed(tab)` | `tabs.read` | A tab closed. |
| `plugin.unload()` | none | The plugin is about to be stopped. |

A plugin must answer `command.invoked` within a time budget. If it does not,
the command fails, the status bar names the plugin, and the UI never waits on
it.

## What a plugin cannot read

A plugin cannot read any of these, with or without a capability:

- cookies, cache, local storage, IndexedDB, and saved passwords or keychain
  items
- page DOM, page scripts, form contents, and network responses
- the filesystem outside its own package, including the profile directory,
  `vrowserrc`, and downloads
- another plugin's storage or messages
- the process command line and environment

## Failure

A plugin host that crashes, sends a malformed message, or exceeds its
message-rate or storage quota is stopped for the rest of the session. Its
commands and mappings are removed, and the status bar says so. Browsing
continues.

## Alternatives considered

**Native shared libraries loaded into the browser process.** These would be
fastest and simplest to call. They would also get every privilege the browser
process has: the profile, cookies, CEF, and the filesystem. They would need a
stable C++ ABI across compilers too. Rejected.

**JavaScript in a renderer, like user scripts.** Chromium's sandbox would
come for free, but the plugin would share a process with page content and
could reach the DOM. Page access is deferred, and when it comes it will be a
separate, narrower API.

**Only `vrowserrc` commands.** `command <name> a | b` already composes
built-in commands with no new trust boundary. That stays the first answer for
simple needs. It cannot hold state or react to events, so it does not replace
plugins.

## Deferred

These are out of version 1. Each one needs its own RFC or issue before it is
added.

- The execution model, language runtime, and sandbox of the plugin host
  (#34).
- Access to page content: DOM, content scripts, user scripts, and injected
  CSS.
- Network access from a plugin, and any filesystem access.
- UI surfaces beyond one status-bar line: panels, completion sources,
  custom hint labels, and the command palette (#31).
- Mappings in modes other than Normal.
- Downloads, permission decisions, profiles, and sessions as plugin data.
- Plugin-to-plugin messaging.
- Installing, updating, signing, and distributing plugins.
- Chrome extension APIs (#38).
- Driving plugins from the local CLI (#36).

## Consequences

- The command registry is the extension point. New built-in features should
  keep landing as named commands, because that is what plugins and keymaps
  can reach.
- Storage stays behind typed stores (`HistoryStore`, `BookmarkStore`). Plugin
  calls map onto those stores, not onto SQL.
- No plugin code lands until #34 picks the execution model, and an issue
  implements one capability at a time.
