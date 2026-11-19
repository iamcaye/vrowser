# Third-party notices

Vrowser itself is licensed under the Apache License 2.0. See `LICENSE`.

The desktop shell links the Chromium Embedded Framework. CEF and Chromium
are not Apache-2.0 code. Distributors of a built Vrowser must ship their
license texts with the application.

## Chromium Embedded Framework

- Project: https://bitbucket.org/chromiumembedded/cef
- Documentation: https://chromiumembedded.github.io/cef/
- Binary builds: https://cef-builds.spotifycdn.com/index.html
- License: BSD 3-Clause. The full text is `LICENSE.txt` inside each official
  binary distribution.
- Vrowser pins the distribution in `cmake/cef_version.cmake` and downloads it
  into `third_party/cef/`, which is gitignored.

The binary distribution also carries Chromium's `CREDITS.html` and the
license set for the bundled codecs, ICU data, and other components. Keep
those files next to the application when packaging. Do not strip the
`Credits` or `LICENSE` files out of the CEF framework.

## GoogleTest

Unit tests fetch GoogleTest 1.16.0 at configure time. GoogleTest is BSD
3-Clause: https://github.com/google/googletest

## Build helpers

`cmake/DownloadCEF.cmake` follows the download flow used by the BSD-licensed
`cef-project` sample (https://github.com/chromiumembedded/cef-project). The
copy in this repository is Vrowser's script and is Apache-2.0.
