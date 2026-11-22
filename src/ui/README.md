# UI

Pure UI logic, no CEF headers. The CEF Views widgets that show it live in
`src/browser/cef`.

- `status_line.cpp`: `FormatStatusLine` builds the status bar text (mode,
  pending keys, URL, loading). `ShortenUrl` keeps the scheme and host,
  drops user info, and cuts the rest from the end.

The command line, tab strip, and link-hint overlays will also live here.
