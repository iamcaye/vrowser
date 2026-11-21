# SPDX-License-Identifier: Apache-2.0
# Shortcuts for the CMake presets. Run `just` to list recipes.

app := "build/debug/apps/desktop/Debug/Vrowser.app"
bin := if os() == "macos" {
    app / "Contents/MacOS/Vrowser"
} else if os() == "windows" {
    "build/debug/apps/desktop/Debug/vrowser.exe"
} else {
    "build/debug/apps/desktop/Debug/vrowser"
}
profile := "build/dev-profile"
# Debug builds are re-signed on every build, so macOS would ask for the
# "Chromium Safe Storage" keychain item on each launch. The dev profile uses
# Chromium's mock keychain instead. Do not use this for a real profile.
dev_flags := if os() == "macos" { "--use-mock-keychain" } else { "" }
mock_keychain := if os() == "macos" { "1" } else { "0" }

# List recipes.
default:
    @just --list

# Configure and build the debug app (downloads CEF on first run).
build:
    cmake --preset debug
    cmake --build --preset debug

# Build, then launch the app. Example: just run https://neovim.io
run url="https://example.com": build
    #!/bin/sh
    set -eu
    if [ "{{ os() }}" != "macos" ]; then
      echo "just run: only the macOS app path is wired up so far" >&2
      exit 1
    fi
    mkdir -p "{{ profile }}"
    exec "{{ bin }}" {{ dev_flags }} --url="{{ url }}" --user-data-dir="$PWD/{{ profile }}"

# Launch the already built app without rebuilding.
open url="https://example.com":
    mkdir -p "{{ profile }}"
    "{{ bin }}" {{ dev_flags }} --url="{{ url }}" --user-data-dir="$PWD/{{ profile }}"

# Build and run the unit tests (no CEF download).
test:
    cmake --preset ci
    cmake --build --preset ci
    ctest --preset ci --output-on-failure

# Build, then open a window, load a page, and expect a clean exit.
smoke: build
    VROWSER_SMOKE_MOCK_KEYCHAIN={{ mock_keychain }} tools/smoke_test.sh "{{ bin }}"

# Build, then send keys to a test page and check Normal and Insert modes.
key-smoke: build
    rm -rf build/key-smoke-profile
    mkdir -p build/key-smoke-profile
    "{{ bin }}" --vrowser-key-smoke-test {{ dev_flags }} \
      --user-data-dir="$PWD/build/key-smoke-profile"

# Build, then open two tabs (Alloy style), close one, and expect a clean exit.
tab-smoke: build
    rm -rf build/tab-smoke-profile
    mkdir -p build/tab-smoke-profile
    "{{ bin }}" --vrowser-tab-smoke-test --use-alloy-style --use-mock-keychain \
      --url=https://example.com --user-data-dir="$PWD/build/tab-smoke-profile"

# Check the macOS bundle layout (helpers and CEF framework).
bundle-check: build
    tools/check_mac_bundle.sh

# Apply clang-format to tracked C++ and Objective-C++ files.
fmt:
    clang-format -i $(git ls-files '*.cc' '*.cpp' '*.h' '*.hpp' '*.mm')

# Remove all build trees.
clean:
    rm -rf build
