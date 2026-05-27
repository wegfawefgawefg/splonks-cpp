#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd sdkmanager

yes | sdkmanager --licenses >/dev/null
sdkmanager \
  "platform-tools" \
  "platforms;android-${ANDROID_API_LEVEL}" \
  "build-tools;${ANDROID_BUILD_TOOLS}" \
  "cmake;3.22.1" \
  "ndk;26.3.11579264" \
  "emulator" \
  "${ANDROID_SYSTEM_IMAGE}"

echo "SDK setup complete."
