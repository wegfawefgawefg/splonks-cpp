#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "run_sim.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
bundle_id="${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_cmd xcrun

"${repo_root}/scripts/ios/build_sim.sh"

app_path="$(find "${repo_root}/build/ios-sim" -type d -name "*.app" | head -n 1)"
if [[ -z "${app_path}" ]]; then
    echo "No simulator .app found under ${repo_root}/build/ios-sim" >&2
    exit 1
fi

device="${SPLONKS_IOS_SIMULATOR_UDID:-}"
if [[ -z "${device}" ]]; then
    booted_line="$(xcrun simctl list devices available | grep "(Booted)" | head -n 1 || true)"
    if [[ "${booted_line}" =~ \(([0-9A-Fa-f-]+)\)\ \(Booted\) ]]; then
        device="${BASH_REMATCH[1]}"
    fi
fi
if [[ -z "${device}" ]]; then
    iphone_line="$(xcrun simctl list devices available | grep "iPhone" | head -n 1 || true)"
    if [[ "${iphone_line}" =~ \(([0-9A-Fa-f-]+)\) ]]; then
        device="${BASH_REMATCH[1]}"
    fi
fi
if [[ -z "${device}" ]]; then
    echo "No available iPhone simulator found. Create one in Xcode or set SPLONKS_IOS_SIMULATOR_UDID." >&2
    exit 1
fi

xcrun simctl boot "${device}" >/dev/null 2>&1 || true
xcrun simctl bootstatus "${device}" -b
xcrun simctl install "${device}" "${app_path}"
xcrun simctl launch "${device}" "${bundle_id}"

echo "[ios] launched ${bundle_id} on simulator ${device}"
