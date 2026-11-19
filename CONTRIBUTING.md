# Contributing

Vrowser changes land through a small pull request tied to one GitHub issue.

## Before you start

1. Read [AGENTS.md](AGENTS.md) and [docs/progress/current-status.md](docs/progress/current-status.md).
2. Pick an open issue labeled `agent-ready` whose dependencies are already
   merged, and which no open pull request already covers.
3. Branch from the default branch:
   `feat/<issue>-<slug>`, `fix/<issue>-<slug>`, or `chore/<issue>-<slug>`.

## While you work

- Stay inside the issue's scope.
- Keep CEF calls on the thread that API documents.
- Leave the sandbox enabled.
- Add or update tests for logic that does not need a window.
- Format C++ with `clang-format`. The style file is `.clang-format`.

## Before you open the pull request

```bash
cmake --preset ci && cmake --build --preset ci && ctest --preset ci --output-on-failure
```

If you touched the desktop shell, also build the `debug` preset and run
`tools/smoke_test.sh` on a machine where CEF is supported. Write down the
platform and the commands that passed. Name any check you could not run.

Use `Closes #<issue>` when the pull request meets every acceptance criterion.
Do not merge it yourself.

Update `docs/progress/current-status.md` when the user-visible behavior or the
recommended next task changes.

## Definition of done

The issue's acceptance criteria are checked, the tests it required were run,
the sandbox and threading rules still hold, and the pull request describes
what was verified.
