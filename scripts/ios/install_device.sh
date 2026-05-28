#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "install_device.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
archive_dir="${repo_root}/dist/splonks-ios"
archive_path="${SPLONKS_IOS_ARCHIVE_PATH:-${archive_dir}/Splonks.xcarchive}"
bundle_id="${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"
device_id="${SPLONKS_IOS_DEVICE_ID:-}"
app_path="${SPLONKS_IOS_DEVICE_APP_PATH:-}"

usage() {
    cat >&2 <<EOF
Usage: $0

Installs and launches the signed iOS archive app on a connected physical
device through Xcode's devicectl. Run scripts/ios/archive_release.sh first.

Environment:
  SPLONKS_IOS_DEVICE_ID       Required physical device identifier.
  SPLONKS_IOS_BUNDLE_ID       Bundle id to launch, default: ${bundle_id}
  SPLONKS_IOS_ARCHIVE_PATH    Archive path, default: ${archive_path}
  SPLONKS_IOS_DEVICE_APP_PATH Optional explicit .app path
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

case "${1:-}" in
    "")
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac

require_cmd xcrun

if [[ -z "${device_id}" ]]; then
    echo "Missing SPLONKS_IOS_DEVICE_ID for physical device validation." >&2
    echo "List devices with: xcrun devicectl list devices" >&2
    exit 1
fi

if [[ -z "${app_path}" ]]; then
    app_path="${archive_path}/Products/Applications/Splonks.app"
fi

if [[ ! -d "${app_path}" ]]; then
    echo "Missing signed iOS .app: ${app_path}" >&2
    echo "Run scripts/ios/archive_release.sh first or set SPLONKS_IOS_DEVICE_APP_PATH." >&2
    exit 1
fi

xcrun devicectl device install app --device "${device_id}" "${app_path}"
xcrun devicectl device process launch --device "${device_id}" "${bundle_id}"

echo "[ios-device] installed ${app_path} on ${device_id}"
echo "[ios-device] launched ${bundle_id} on ${device_id}"
