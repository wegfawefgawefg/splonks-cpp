#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ANDROID_DIR="${REPO_ROOT}/android"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_SDK_ROOT/ndk/26.3.11579264}"
export ANDROID_API_LEVEL="${ANDROID_API_LEVEL:-34}"
export ANDROID_BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-34.0.0}"
export ANDROID_SYSTEM_IMAGE="${ANDROID_SYSTEM_IMAGE:-system-images;android-${ANDROID_API_LEVEL};google_apis;x86_64}"
export ANDROID_AVD_NAME="${ANDROID_AVD_NAME:-splonks-api${ANDROID_API_LEVEL}}"

export APP_ID="dev.splonks.game"
export APP_ACTIVITY="dev.splonks.game.SplonksActivity"

for dir in \
    "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin" \
    "${ANDROID_SDK_ROOT}/platform-tools" \
    "${ANDROID_SDK_ROOT}/emulator"; do
    if [[ -d "${dir}" ]]; then
        export PATH="${dir}:${PATH}"
    fi
done

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_sdl3_aar() {
    if ! compgen -G "${ANDROID_DIR}/app/libs/SDL3*.aar" >/dev/null; then
        echo "Missing SDL3 Android AAR in ${ANDROID_DIR}/app/libs" >&2
        echo "Download an official SDL3 Android archive and place it there before building." >&2
        exit 1
    fi
}
