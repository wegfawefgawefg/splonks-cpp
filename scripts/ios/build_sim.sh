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
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
bundle_id="${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"
cd "${repo_root}"

cmake --preset ios-sim \
    -DSPLONKS_BUNDLE_VERSION="${version}" \
    -DSPLONKS_IOS_BUNDLE_ID="${bundle_id}"
cmake --build --preset ios-sim

echo "[ios] simulator app built under build/ios-sim"
