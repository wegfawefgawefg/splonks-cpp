#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd emulator
require_cmd adb

nohup emulator -avd "${ANDROID_AVD_NAME}" >/tmp/splonks-emulator.log 2>&1 &
adb wait-for-device
adb devices

echo "Emulator started: ${ANDROID_AVD_NAME}"
echo "Logs: /tmp/splonks-emulator.log"
