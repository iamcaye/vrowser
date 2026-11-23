# Storage

SQLite storage. `vrowser_sqlite` builds the official amalgamation pinned in
`CMakeLists.txt` by SHA3-256, with extension loading compiled out.

`ApplyMigrations` (`include/vrowser/storage/migrations.hpp`) brings a database
file up to the latest schema. Migrations are numbered 1..N; the applied
version is SQLite's `PRAGMA user_version`. Each migration runs in one
transaction with its version update, so a failure leaves the previous version.

History, bookmarks, and saved sessions will add migrations here. Schema
changes go through the migration list, not ad-hoc SQL in the browser code.
