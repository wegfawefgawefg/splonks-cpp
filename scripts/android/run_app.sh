#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd adb
adb shell am start -n "${APP_ID}/${APP_ACTIVITY}"
