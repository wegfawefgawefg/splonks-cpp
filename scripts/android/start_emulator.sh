#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd emulator
require_cmd adb

emulator_flags=(
    -avd "${ANDROID_AVD_NAME}"
    -no-window
    -no-audio
    -no-boot-anim
    -no-snapshot
    -gpu swiftshader_indirect
)

if command -v setsid >/dev/null 2>&1; then
    setsid -f emulator "${emulator_flags[@]}" >/tmp/splonks-emulator.log 2>&1 < /dev/null
else
    nohup emulator "${emulator_flags[@]}" >/tmp/splonks-emulator.log 2>&1 < /dev/null &
fi
adb wait-for-device

deadline=$((SECONDS + ${ANDROID_EMULATOR_BOOT_TIMEOUT_S:-180}))
while (( SECONDS < deadline )); do
    boot_completed="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)"
    if [[ "${boot_completed}" == "1" ]]; then
        adb devices
        echo "Emulator started: ${ANDROID_AVD_NAME}"
        echo "Logs: /tmp/splonks-emulator.log"
        exit 0
    fi
    sleep 2
done

echo "Timed out waiting for emulator boot: ${ANDROID_AVD_NAME}" >&2
echo "Logs: /tmp/splonks-emulator.log" >&2
tail -100 /tmp/splonks-emulator.log >&2 || true
exit 1
