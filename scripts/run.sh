#!/usr/bin/env bash
set -euo pipefail

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

"$(dirname "$0")"/build.sh

binary_dir="build"
if [ "${SPLONKS_PRESET:-release}" = "dev" ]; then
    binary_dir="build-debug"
fi

"$(dirname "$0")"/../${binary_dir}/splonks-cpp
