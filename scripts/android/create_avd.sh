#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd avdmanager

if avdmanager list avd | grep -q "Name: ${ANDROID_AVD_NAME}"; then
    echo "AVD already exists: ${ANDROID_AVD_NAME}"
    exit 0
fi

echo "no" | avdmanager create avd -n "${ANDROID_AVD_NAME}" -k "${ANDROID_SYSTEM_IMAGE}"
echo "Created AVD: ${ANDROID_AVD_NAME}"
