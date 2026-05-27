#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "build_sim.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Missing Xcode command line tools. Run: xcode-select --install" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
cd "${repo_root}"

cmake --preset ios-sim
cmake --build --preset ios-sim

echo "[ios] simulator app built under build/ios-sim"
