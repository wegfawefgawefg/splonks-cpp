#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ANDROID_DIR="${REPO_ROOT}/android"

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
export ANDROID_HOME="${ANDROID_HOME:-$ANDROID_SDK_ROOT}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_SDK_ROOT/ndk/26.3.11579264}"
export ANDROID_API_LEVEL="${ANDROID_API_LEVEL:-34}"
export ANDROID_BUILD_TOOLS="${ANDROID_BUILD_TOOLS:-34.0.0}"
export ANDROID_SYSTEM_IMAGE="${ANDROID_SYSTEM_IMAGE:-system-images;android-${ANDROID_API_LEVEL};google_apis;x86_64}"
export ANDROID_AVD_NAME="${ANDROID_AVD_NAME:-splonks-api${ANDROID_API_LEVEL}}"
export SDL3_ANDROID_VERSION="${SDL3_ANDROID_VERSION:-3.4.0}"
export SDL3_ANDROID_ZIP_SHA256="${SDL3_ANDROID_ZIP_SHA256:-ed8e9278b4a944fc0ad93ece64cfc6d46693eaa4d47a5f87d891a3c24c783c21}"

export APP_ID="dev.splonks.game"
export APP_ACTIVITY="dev.splonks.game.SplonksActivity"

if [[ -z "${JAVA_HOME:-}" ]] && command -v javac >/dev/null 2>&1; then
    javac_path="$(readlink -f "$(command -v javac)" 2>/dev/null || command -v javac)"
    export JAVA_HOME="$(cd "$(dirname "${javac_path}")/.." && pwd)"
fi

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
        echo "Run scripts/android/fetch_sdl3_aar.sh before building." >&2
        exit 1
    fi
}

require_android_ndk() {
    local toolchain="${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"
    if [[ ! -f "${toolchain}" ]]; then
        echo "Missing Android NDK toolchain: ${toolchain}" >&2
        echo "Run scripts/android/setup_sdk.sh or set ANDROID_NDK_HOME to an installed NDK." >&2
        exit 1
    fi
}

require_java() {
    if [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/java" ]]; then
        if [[ ! -x "${JAVA_HOME}/bin/javac" || ! -x "${JAVA_HOME}/bin/jlink" ]]; then
            echo "JAVA_HOME must point to a full JDK with javac and jlink: ${JAVA_HOME}" >&2
            exit 1
        fi
        return 0
    fi
    if command -v java >/dev/null 2>&1 && command -v javac >/dev/null 2>&1 && command -v jlink >/dev/null 2>&1; then
        return 0
    fi
    echo "Missing Java JDK. Install JDK 17+ or set JAVA_HOME before running Android Gradle tasks." >&2
    exit 1
}
