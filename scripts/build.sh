#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build"
mode="${1:-build}"
preset="${SPLONKS_PRESET:-release}"
jobs="${SPLONKS_BUILD_JOBS:-2}"

if ! [[ "${jobs}" =~ ^[0-9]+$ ]] || [ "${jobs}" -lt 1 ]; then
    echo "SPLONKS_BUILD_JOBS must be a positive integer; got '${jobs}'." >&2
    exit 2
fi

configure() {
    cmake --preset "${preset}"
}

if ! configure; then
    rm -f "${build_dir}/CMakeCache.txt"
    rm -rf "${build_dir}/CMakeFiles"
    configure
fi

if [ "${mode}" = "--configure-only" ]; then
    exit 0
fi

cmake --build --preset "${preset}" -j "${jobs}"
