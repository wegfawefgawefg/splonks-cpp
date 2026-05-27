#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "notarize_app.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
app_dir="${repo_root}/dist/splonks-macos/Splonks.app"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"
signed_zip="${release_dir}/splonks-${version}-macos-notarization.zip"
artifact_zip="${release_dir}/splonks-${version}-macos-arm64.zip"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_cmd xcrun
require_cmd codesign
require_cmd ditto
require_cmd spctl
require_cmd xattr

if [[ ! -d "${app_dir}" ]]; then
    echo "Missing app bundle: ${app_dir}" >&2
    echo "Run scripts/package_macos.sh and scripts/verify_package_macos.sh first." >&2
    exit 1
fi

if [[ -z "${SPLONKS_MACOS_SIGN_IDENTITY:-}" ]]; then
    echo "Missing SPLONKS_MACOS_SIGN_IDENTITY, for example: Developer ID Application: Name (TEAMID)" >&2
    exit 1
fi

mkdir -p "${release_dir}"

verify_quarantined_archive() {
    local temp_dir
    local extracted_app
    temp_dir="$(mktemp -d)"
    ditto -x -k "${artifact_zip}" "${temp_dir}"
    extracted_app="${temp_dir}/Splonks.app"
    if [[ ! -d "${extracted_app}" ]]; then
        echo "Notarized archive did not extract Splonks.app from ${artifact_zip}" >&2
        rm -rf "${temp_dir}"
        exit 1
    fi

    xattr -w com.apple.quarantine "0081;$(printf '%x' "$(date +%s)");Splonks;$(basename "${artifact_zip}")" "${extracted_app}"
    spctl --assess --type execute --verbose=2 "${extracted_app}"
    "${extracted_app}/Contents/MacOS/Splonks" \
        --check-state-fingerprint-smoke \
        --project-root "${extracted_app}/Contents/Resources" \
        >"${temp_dir}/smoke.txt"
    grep -q "state fingerprint smoke ok" "${temp_dir}/smoke.txt"
    rm -rf "${temp_dir}"
    echo "[notarize] quarantined extracted archive launch ok"
}

codesign --force --deep --options runtime --timestamp \
    --sign "${SPLONKS_MACOS_SIGN_IDENTITY}" \
    "${app_dir}"

codesign --verify --deep --strict --verbose=2 "${app_dir}"
spctl --assess --type execute --verbose=2 "${app_dir}" || true

rm -f "${signed_zip}" "${artifact_zip}"
ditto -c -k --keepParent "${app_dir}" "${signed_zip}"

if [[ -n "${SPLONKS_NOTARYTOOL_PROFILE:-}" ]]; then
    xcrun notarytool submit "${signed_zip}" \
        --keychain-profile "${SPLONKS_NOTARYTOOL_PROFILE}" \
        --wait
else
    required_env=(
        APPLE_ID
        APPLE_TEAM_ID
        APPLE_APP_SPECIFIC_PASSWORD
    )
    for name in "${required_env[@]}"; do
        if [[ -z "${!name:-}" ]]; then
            echo "Missing ${name} for notarytool submission." >&2
            echo "Set SPLONKS_NOTARYTOOL_PROFILE or Apple ID env vars." >&2
            exit 1
        fi
    done
    xcrun notarytool submit "${signed_zip}" \
        --apple-id "${APPLE_ID}" \
        --team-id "${APPLE_TEAM_ID}" \
        --password "${APPLE_APP_SPECIFIC_PASSWORD}" \
        --wait
fi

xcrun stapler staple "${app_dir}"
xcrun stapler validate "${app_dir}"

ditto -c -k --keepParent "${app_dir}" "${artifact_zip}"
if command -v shasum >/dev/null 2>&1; then
    (cd "${release_dir}" && shasum -a 256 "$(basename "${artifact_zip}")" > "$(basename "${artifact_zip}").sha256")
fi

verify_quarantined_archive

echo "[notarize] ${artifact_zip}"
echo "[notarize] ${artifact_zip}.sha256"
