#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"
archive_dir="${repo_root}/dist/splonks-ios"
manifest_path="${archive_dir}/manifest.txt"
ipa_path="${SPLONKS_IOS_IPA_PATH:-${release_dir}/splonks-${version}-ios.ipa}"
checksum_path="${ipa_path}.sha256"

usage() {
    cat >&2 <<EOF
Usage: $0

Verifies the exported iOS IPA, checksum file, release manifest, and expected
bundled Splonks content.

Environment:
  SPLONKS_RELEASE_VERSION   Version in the default IPA path, default: ${version}
  SPLONKS_IOS_IPA_PATH      Optional explicit IPA path
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

manifest_value() {
    local key="$1"
    awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${manifest_path}" | tail -n 1
}

require_manifest_value() {
    local key="$1"
    local expected="$2"
    local actual
    actual="$(manifest_value "${key}")"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "Manifest ${key} mismatch: expected '${expected}', got '${actual}'" >&2
        exit 1
    fi
}

require_listing_match() {
    local description="$1"
    local pattern="$2"
    if ! grep -Eq "${pattern}" <<<"${listing}"; then
        echo "Missing IPA entry: ${description}" >&2
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

require_cmd unzip

if [[ ! -f "${ipa_path}" ]]; then
    echo "Missing iOS IPA: ${ipa_path}" >&2
    exit 1
fi
if [[ ! -f "${manifest_path}" ]]; then
    echo "Missing iOS release manifest: ${manifest_path}" >&2
    exit 1
fi
if [[ ! -f "${checksum_path}" ]]; then
    echo "Missing iOS IPA checksum: ${checksum_path}" >&2
    exit 1
fi

require_manifest_value name splonks
require_manifest_value platform ios
require_manifest_value mode release
require_manifest_value artifact "$(basename "${ipa_path}")"
require_manifest_value version_name "${version}"

expected_sha="$(manifest_value sha256)"
if [[ -z "${expected_sha}" ]]; then
    echo "Manifest is missing sha256" >&2
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha="$(sha256sum "${ipa_path}" | awk '{print $1}')"
elif command -v shasum >/dev/null 2>&1; then
    actual_sha="$(shasum -a 256 "${ipa_path}" | awk '{print $1}')"
else
    echo "Missing sha256sum or shasum" >&2
    exit 1
fi
if [[ "${actual_sha}" != "${expected_sha}" ]]; then
    echo "IPA SHA-256 mismatch: expected ${expected_sha}, got ${actual_sha}" >&2
    exit 1
fi

checksum_sha="$(awk '{print $1}' "${checksum_path}")"
if [[ "${checksum_sha}" != "${actual_sha}" ]]; then
    echo "Checksum file mismatch: expected ${actual_sha}, got ${checksum_sha}" >&2
    exit 1
fi

listing="$(unzip -Z1 "${ipa_path}")"
require_listing_match "Payload app Info.plist" '^Payload/[^/]+\.app/Info\.plist$'
require_listing_match "Payload app executable" '^Payload/[^/]+\.app/splonks-cpp$'
require_listing_match "bundled font asset" '^Payload/[^/]+\.app/(.*/)?assets/fonts/DejaVuSans\.ttf$'
require_listing_match "bundled graphics annotations" '^Payload/[^/]+\.app/(.*/)?assets/graphics/annotations\.yaml$'
require_listing_match "bundled audio annotations" '^Payload/[^/]+\.app/(.*/)?assets/audio/annotations\.yaml$'
require_listing_match "bundled settings data" '^Payload/[^/]+\.app/(.*/)?data/settings\.cfg$'

echo "[verify-ipa] ${ipa_path} ok"
echo "[verify-ipa] sha256=${actual_sha}"
