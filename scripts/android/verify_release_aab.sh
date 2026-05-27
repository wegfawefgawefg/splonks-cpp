#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

version_name="${SPLONKS_ANDROID_VERSION_NAME:-${SPLONKS_RELEASE_VERSION:-0.1.0}}"
dist_dir="${REPO_ROOT}/dist/splonks-android"
manifest_path="${dist_dir}/manifest.txt"
aab_path="${SPLONKS_ANDROID_AAB_PATH:-${dist_dir}/splonks-${version_name}-android-release.aab}"

require_cmd unzip
require_cmd jarsigner

if [[ ! -f "${aab_path}" ]]; then
    echo "Missing Android release AAB: ${aab_path}" >&2
    exit 1
fi
if [[ ! -f "${manifest_path}" ]]; then
    echo "Missing Android release manifest: ${manifest_path}" >&2
    exit 1
fi

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

require_manifest_value name splonks
require_manifest_value platform android
require_manifest_value mode release
require_manifest_value artifact "$(basename "${aab_path}")"

expected_sha="$(manifest_value sha256)"
if [[ -z "${expected_sha}" ]]; then
    echo "Manifest is missing sha256" >&2
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha="$(sha256sum "${aab_path}" | awk '{print $1}')"
else
    require_cmd shasum
    actual_sha="$(shasum -a 256 "${aab_path}" | awk '{print $1}')"
fi
if [[ "${actual_sha}" != "${expected_sha}" ]]; then
    echo "AAB SHA-256 mismatch: expected ${expected_sha}, got ${actual_sha}" >&2
    exit 1
fi

listing="$(unzip -Z1 "${aab_path}")"
required_entries=(
    "BundleConfig.pb"
    "base/manifest/AndroidManifest.xml"
    "base/dex/classes.dex"
    "base/lib/arm64-v8a/libmain.so"
    "base/lib/arm64-v8a/libSDL3.so"
    "base/lib/arm64-v8a/libSDL3_image.so"
    "base/lib/arm64-v8a/libSDL3_mixer.so"
    "base/lib/arm64-v8a/libSDL3_ttf.so"
    "base/assets/assets/fonts/DejaVuSans.ttf"
    "base/assets/assets/graphics/annotations.yaml"
    "base/assets/assets/audio/annotations.yaml"
    "base/assets/data/settings.cfg"
)

for entry in "${required_entries[@]}"; do
    if ! grep -Fxq "${entry}" <<<"${listing}"; then
        echo "Missing AAB entry: ${entry}" >&2
        exit 1
    fi
done

jarsigner -verify "${aab_path}" >/tmp/splonks-aab-jarsigner.txt 2>&1 || {
    cat /tmp/splonks-aab-jarsigner.txt >&2
    exit 1
}
if ! grep -Fq "jar verified." /tmp/splonks-aab-jarsigner.txt; then
    cat /tmp/splonks-aab-jarsigner.txt >&2
    echo "AAB JAR signature verification did not report success" >&2
    exit 1
fi

echo "[verify-aab] ${aab_path} ok"
echo "[verify-aab] sha256=${actual_sha}"
