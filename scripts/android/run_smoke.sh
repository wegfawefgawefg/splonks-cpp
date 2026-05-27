#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd adb

timeout_s="${ANDROID_SMOKE_TIMEOUT_S:-30}"
extra_name="dev.splonks.game.ARGS"
smoke_args="${SPLONKS_ANDROID_SMOKE_ARGS:---check-state-fingerprint-smoke}"
success_pattern="${SPLONKS_ANDROID_SMOKE_PATTERN:-state fingerprint smoke ok}"

adb logcat -c
adb shell am force-stop "${APP_ID}" >/dev/null 2>&1 || true
adb shell am start \
    -n "${APP_ID}/${APP_ACTIVITY}" \
    --es "${extra_name}" "${smoke_args}" \
    >/dev/null

deadline=$((SECONDS + timeout_s))
tmp_log="$(mktemp)"
cleanup() {
    rm -f "${tmp_log}"
}
trap cleanup EXIT

while (( SECONDS < deadline )); do
    adb logcat -d > "${tmp_log}"
    if grep -q "${success_pattern}" "${tmp_log}"; then
        echo "[android-smoke] ${success_pattern}"
        exit 0
    fi
    if grep -Eiq "AndroidRuntime|FATAL EXCEPTION|signal [0-9]+|Fatal signal" "${tmp_log}"; then
        grep -Ei "AndroidRuntime|FATAL EXCEPTION|signal [0-9]+|Fatal signal|splonks|SDL" "${tmp_log}" >&2 || true
        exit 1
    fi
    sleep 1
done

echo "[android-smoke] timed out waiting for: ${success_pattern}" >&2
grep -Ei "AndroidRuntime|FATAL|splonks|SDL|main" "${tmp_log}" >&2 || true
exit 1
