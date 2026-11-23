# Storage

SQLite storage. `vrowser_sqlite` builds the official amalgamation pinned in
`CMakeLists.txt` by SHA3-256, with extension loading compiled out.

`ApplyMigrations` (`include/vrowser/storage/migrations.hpp`) brings a database
file up to the latest schema. Migrations are numbered 1..N; the applied
version is SQLite's `PRAGMA user_version`. Each migration runs in one
transaction with its version update, so a failure leaves the previous version.

History (`include/vrowser/storage/history.hpp`): the profile database
`vrowser.db` in the profile directory has a `history` table (URL, title,
visit time in ms since the epoch). `HistoryRecorder` writes one row per
committed main-frame navigation to an `http` or `https` URL, and fills the
title in when the page reports it. Subframes, `data:` error pages, and
internal pages are not recorded. To look at it:

```bash
sqlite3 <profile>/vrowser.db \
  "SELECT url, title, datetime(visited_at_ms / 1000, 'unixepoch') FROM history ORDER BY visited_at_ms DESC LIMIT 20"
```

Bookmarks and saved sessions will add migrations to the same list. Schema
changes go through the migration list, not ad-hoc SQL in the browser code.
