#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_sdl3_aar
require_android_ndk
cd "${REPO_ROOT}"
cmake --preset android-arm64
cmake --build --preset android-arm64
