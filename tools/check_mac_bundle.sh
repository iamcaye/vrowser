#!/usr/bin/env bash
# Copyright 2026 The Vrowser Authors
# SPDX-License-Identifier: Apache-2.0
#
# Fail when the macOS app is missing the main executable, a helper bundle, or
# the Chromium Embedded Framework. The sandbox layout depends on all of them.

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/check_mac_bundle.sh [path/to/Vrowser.app]

Defaults to build/debug/apps/desktop/Debug/Vrowser.app when no path is given.
Exits 0 when the layout matches CEF's macOS helper layout; exits 1 otherwise.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="${1:-${ROOT}/build/debug/apps/desktop/Debug/Vrowser.app}"

HELPERS=(
  "Vrowser Helper"
  "Vrowser Helper (GPU)"
  "Vrowser Helper (Plugin)"
  "Vrowser Helper (Renderer)"
  "Vrowser Helper (Alerts)"
)

errors=0

fail() {
  echo "error: $*" >&2
  errors=$((errors + 1))
}

require_file() {
  local path="$1"
  local label="$2"
  if [[ ! -f "${path}" ]]; then
    fail "missing ${label}: ${path}"
    return
  fi
  if [[ ! -x "${path}" ]]; then
    fail "not executable ${label}: ${path}"
  fi
}

require_dir() {
  local path="$1"
  local label="$2"
  if [[ ! -d "${path}" ]]; then
    fail "missing ${label}: ${path}"
  fi
}

if [[ ! -d "${APP}" ]]; then
  echo "error: app bundle not found: ${APP}" >&2
  exit 1
fi

echo "Checking ${APP}"

require_file "${APP}/Contents/MacOS/Vrowser" "main executable"

for helper in "${HELPERS[@]}"; do
  helper_app="${APP}/Contents/Frameworks/${helper}.app"
  require_dir "${helper_app}" "helper bundle '${helper}.app'"
  require_file "${helper_app}/Contents/MacOS/${helper}" \
    "helper executable '${helper}'"
done

framework="${APP}/Contents/Frameworks/Chromium Embedded Framework.framework"
require_dir "${framework}" "Chromium Embedded Framework.framework"
if [[ ! -e "${framework}/Chromium Embedded Framework" ]]; then
  fail "missing framework binary: ${framework}/Chromium Embedded Framework"
fi

if [[ "${errors}" -ne 0 ]]; then
  echo "macOS bundle check failed with ${errors} error(s)" >&2
  exit 1
fi

echo "macOS bundle check passed"
