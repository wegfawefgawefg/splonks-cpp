#!/usr/bin/env bash
set -euo pipefail

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

"$(dirname "$0")"/build.sh
"$(dirname "$0")"/../build/splonks-cpp
