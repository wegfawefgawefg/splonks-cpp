#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

preset="${SPLONKS_PRESET:-release}"
jobs="${SPLONKS_LAPTOP_JOBS:-2}"

if ! [[ "${jobs}" =~ ^[0-9]+$ ]] || [ "${jobs}" -lt 1 ]; then
    echo "SPLONKS_LAPTOP_JOBS must be a positive integer; got '${jobs}'." >&2
    exit 2
fi

configure() {
    cmake --preset "${preset}"
}

cd "${repo_root}"
if ! configure; then
    build_dir="build"
    if [ "${preset}" = "dev" ]; then
        build_dir="build-debug"
    fi
    rm -f "${repo_root}/${build_dir}/CMakeCache.txt"
    rm -rf "${repo_root}/${build_dir}/CMakeFiles"
    configure
fi

cmake --build --preset "${preset}" -j "${jobs}"
