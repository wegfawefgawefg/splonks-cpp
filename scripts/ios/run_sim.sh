#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "run_sim.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
bundle_id="${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"

usage() {
    cat >&2 <<EOF
Usage: $0 [game arguments...]

Builds, installs, and launches the iOS simulator app. Any extra arguments are
forwarded to the app. When --check-state-fingerprint-smoke is present, this
script attaches to simulator stdout/stderr and requires the smoke success line.
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

case "${1:-}" in
    -h|--help|help)
        usage
        exit 0
        ;;
esac

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

requires_smoke=0
for arg in "$@"; do
    if [[ "${arg}" == "--check-state-fingerprint-smoke" ]]; then
        requires_smoke=1
    fi
done

if [[ "${requires_smoke}" -eq 1 ]]; then
    smoke_log="$(mktemp)"
    xcrun simctl launch --console --terminate-running-process "${device}" "${bundle_id}" "$@" \
        2>&1 | tee "${smoke_log}"
    if ! grep -q "state fingerprint smoke ok" "${smoke_log}"; then
        echo "iOS simulator smoke did not report success." >&2
        rm -f "${smoke_log}"
        exit 1
    fi
    rm -f "${smoke_log}"
    echo "[ios] simulator runtime smoke ok on ${device}"
else
    xcrun simctl launch --terminate-running-process "${device}" "${bundle_id}" "$@"
    echo "[ios] launched ${bundle_id} on simulator ${device}"
fi
