#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
"${root}/scripts/android/build_native.sh"
"${root}/scripts/android/build_apk.sh"
"${root}/scripts/android/install_apk.sh"
"${root}/scripts/android/run_smoke.sh"
"${root}/scripts/android/run_app.sh"
