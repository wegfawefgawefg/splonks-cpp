#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEODRIVER=x11
fi

"${repo_root}/scripts/build.sh"

binary_dir="build"
if [ "${SPLONKS_PRESET:-release}" = "dev" ]; then
    binary_dir="build-debug"
fi

binary="${repo_root}/${binary_dir}/splonks-cpp"
case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*)
        binary="${binary}.exe"
        ;;
esac

if [[ ! -x "${binary}" ]]; then
    echo "Built binary not found or not executable: ${binary}" >&2
    exit 1
fi

"${binary}" "$@" --project-root "${repo_root}"
