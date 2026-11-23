# Config

`LoadUserConfig` (`include/vrowser/config/user_config.hpp`) reads
`vrowserrc` from the profile directory (`--user-data-dir`) at startup and
applies it to the Normal-mode key bindings after the defaults.

```text
" Comments start with " or # and take a whole line.
" Bind keys to a registered command, with optional arguments.
map J tab-previous
map ,n open https://neovim.io
" Remove a binding.
unmap x
```

Rules:

- One entry per line. A comment cannot follow an entry on the same line. `map <keys> <command> [args...]` replaces an existing
  binding for the same keys. `unmap <keys>` removes one.
- The command must be registered in the command registry.
- The whole file applies or none of it does. An error such as
  `vrowserrc:3: Unknown command: foo` keeps the previous bindings and shows
  in the status bar (and on stderr).
- A missing file keeps the defaults. Files over 1 MiB are rejected.

Settings other than keymaps are not supported yet. Do not invent a plugin
loader here.
