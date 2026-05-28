#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd adb
if (($# > 0)); then
    adb shell am start \
        -n "${APP_ID}/${APP_ACTIVITY}" \
        --es "dev.splonks.game.ARGS" "$*"
else
    adb shell am start -n "${APP_ID}/${APP_ACTIVITY}"
fi
