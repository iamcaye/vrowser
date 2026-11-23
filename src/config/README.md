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
" A new command that runs other commands in order; steps are split by |.
command top-reload scroll-to-top | reload
map R top-reload
```

Rules:

- One entry per line. A comment cannot follow an entry on the same line. `map <keys> <command> [args...]` replaces an existing
  binding for the same keys. `unmap <keys>` removes one.
- `command <name> <step> [| <step>...]` defines a command. Each step is a
  command line (name and arguments). The steps run in order and stop at the
  first error. The name must be new, and each step must name a registered
  command or one defined on an earlier line, so a command cannot call
  itself. Custom commands take no arguments of their own.
- Commands named by `map` must be registered, or defined earlier with
  `command`.
- The whole file applies or none of it does, commands included. An error such as
  `vrowserrc:3: Unknown command: foo` keeps the previous bindings and shows
  in the status bar (and on stderr).
- A missing file keeps the defaults. Files over 1 MiB are rejected.

Settings other than keymaps are not supported yet. Do not invent a plugin
loader here.
