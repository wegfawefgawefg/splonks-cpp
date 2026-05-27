#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_java
require_cmd sdkmanager

set +o pipefail
yes | sdkmanager --licenses >/dev/null
set -o pipefail
sdkmanager \
  "platform-tools" \
  "platforms;android-${ANDROID_API_LEVEL}" \
  "build-tools;${ANDROID_BUILD_TOOLS}" \
  "cmake;3.22.1" \
  "ndk;26.3.11579264" \
  "emulator" \
  "${ANDROID_SYSTEM_IMAGE}"

echo "SDK setup complete."
