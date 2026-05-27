#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_java
require_cmd sdkmanager

packages=(
  "platform-tools"
  "platforms;android-${ANDROID_API_LEVEL}"
  "build-tools;${ANDROID_BUILD_TOOLS}"
  "cmake;3.22.1"
  "ndk;26.3.11579264"
  "emulator"
  "${ANDROID_SYSTEM_IMAGE}"
)

(yes || true) | sdkmanager --sdk_root="${ANDROID_SDK_ROOT}" --licenses
(yes || true) | sdkmanager --sdk_root="${ANDROID_SDK_ROOT}" "${packages[@]}"

required_paths=(
  "${ANDROID_SDK_ROOT}/platform-tools/adb"
  "${ANDROID_SDK_ROOT}/platforms/android-${ANDROID_API_LEVEL}/android.jar"
  "${ANDROID_SDK_ROOT}/build-tools/${ANDROID_BUILD_TOOLS}/aapt2"
  "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake"
)

for path in "${required_paths[@]}"; do
  if [[ ! -e "${path}" ]]; then
    echo "SDK setup did not produce required path: ${path}" >&2
    exit 1
  fi
done

echo "SDK setup complete."
