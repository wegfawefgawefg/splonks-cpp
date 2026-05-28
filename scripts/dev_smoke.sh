#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
preset="${SPLONKS_PRESET:-dev}"

case "${preset}" in
    dev)
        build_dir="${repo_root}/build-debug"
        ;;
    release)
        build_dir="${repo_root}/build"
        ;;
    *)
        echo "dev_smoke.sh supports SPLONKS_PRESET=dev or release, got: ${preset}" >&2
        exit 1
        ;;
esac

"${repo_root}/scripts/build.sh"

binary="${build_dir}/splonks-cpp"
case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*)
        binary="${binary}.exe"
        ;;
esac

if [[ ! -x "${binary}" ]]; then
    echo "Built binary not found or not executable: ${binary}" >&2
    exit 1
fi

"${binary}" --check-state-fingerprint-smoke --project-root "${repo_root}"

echo "[dev-smoke] ${preset} build and headless smoke ok"
