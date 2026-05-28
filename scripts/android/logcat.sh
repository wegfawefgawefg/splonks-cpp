#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd adb
if command -v rg >/dev/null 2>&1; then
    adb logcat | rg -i "SDL|AndroidRuntime|FATAL|${APP_ID}|main|splonks"
else
    adb logcat | grep -Ei "SDL|AndroidRuntime|FATAL|${APP_ID}|main|splonks"
fi
