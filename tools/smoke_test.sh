#!/bin/sh
# Opens Vrowser, loads a page, and expects the process to exit.
# Usage: tools/smoke_test.sh [path/to/Vrowser.app]
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
app=${1:-"$root/build/debug/apps/desktop/Debug/Vrowser.app"}
bin="$app/Contents/MacOS/Vrowser"
profile=${VROWSER_SMOKE_PROFILE:-"$root/build/smoke-profile"}
url=${VROWSER_SMOKE_URL:-https://example.com}
rm -rf "$profile"
mkdir -p "$profile"

if [ ! -x "$bin" ]; then
  echo "smoke: missing executable: $bin" >&2
  exit 1
fi

echo "smoke: profile $profile"
set +e
# Set VROWSER_SMOKE_MOCK_KEYCHAIN=1 when the macOS login keychain cannot
# present a dialog. Normal launches do not pass that Chromium switch.
keychain_args=""
if [ "${VROWSER_SMOKE_MOCK_KEYCHAIN:-}" = "1" ]; then
  keychain_args="--use-mock-keychain"
fi
"$bin" \
  --vrowser-smoke-test \
  $keychain_args \
  --url="$url" \
  --user-data-dir="$profile"
status=$?
set -e

if [ "$status" -ne 0 ]; then
  echo "smoke: process exited $status" >&2
  exit "$status"
fi

echo "smoke: passed"
