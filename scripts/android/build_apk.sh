#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_sdl3_aar
require_android_ndk
require_java
cd "${ANDROID_DIR}"
if [[ -x ./gradlew ]]; then
    ./gradlew assembleDebug
elif command -v gradle >/dev/null 2>&1; then
    gradle assembleDebug
else
    echo "Missing Gradle. Install gradle or add a Gradle wrapper under android/." >&2
    exit 1
fi
