# Third party

`third_party/cef/` is created by CMake and is gitignored. It holds the
official CEF binary distribution pinned in `cmake/cef_version.cmake`.

Do not commit that tree. Distributors copy its license files into the
application package.
