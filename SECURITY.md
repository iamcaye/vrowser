# Security

Vrowser renders untrusted web content. The security model is described in
[docs/architecture/security-model.md](docs/architecture/security-model.md).

## Reporting

Email the maintainers with the details, or open a private GitHub security
advisory if this repository has advisories enabled. Give the maintainers time
to acknowledge the report before describing a sandbox escape, a universal
XSS, or a way to read another profile's data in a public issue.

Include the Vrowser commit, the CEF version from `cmake/cef_version.cmake`,
the operating system, and a minimal way to reproduce the behavior.

## What stays on

- The CEF sandbox (`USE_SANDBOX=ON`).
- Chromium certificate and site-isolation behavior.
- A Vrowser-specific `root_cache_path`, so this browser does not share a
  profile directory with another CEF application.

Builds that pass `-DUSE_SANDBOX=OFF` are local debugging builds. Do not ship
them.
