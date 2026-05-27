#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "verify_package_macos.sh must run on macOS" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/splonks-macos"
app_dir="${dist_dir}/Splonks.app"
frameworks_dir="${app_dir}/Contents/Frameworks"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
bundle_id="${SPLONKS_MACOS_BUNDLE_ID:-dev.splonks.game}"

"${repo_root}/scripts/package_macos.sh"

required_files=(
    "${app_dir}/Contents/MacOS/Splonks"
    "${app_dir}/Contents/MacOS/splonks-bin"
    "${app_dir}/Contents/Info.plist"
    "${app_dir}/Contents/Resources/assets/fonts/DejaVuSans.ttf"
    "${app_dir}/Contents/Resources/assets/graphics/annotations.yaml"
    "${app_dir}/Contents/Resources/assets/audio/annotations.yaml"
    "${app_dir}/Contents/Resources/data/settings.cfg"
    "${dist_dir}/PACKAGE_MANIFEST.txt"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "[verify-package] missing ${path}" >&2
        exit 1
    fi
done

grep -Fxq "app=splonks" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -Fxq "platform=macos" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -Fxq "mode=release" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -Fxq "release_version=${version}" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -Fq "<string>${bundle_id}</string>" "${app_dir}/Contents/Info.plist"
grep -Fq "<string>${version}</string>" "${app_dir}/Contents/Info.plist"

require_dylib() {
    local label="$1"
    local pattern="$2"
    if ! find "${frameworks_dir}" -type f -name "${pattern}" | grep -q .; then
        echo "[verify-package] missing bundled ${label} dylib in ${frameworks_dir}" >&2
        exit 1
    fi
}

require_dylib "SDL3" "*SDL3*.dylib"
require_dylib "SDL3_image" "*SDL3_image*.dylib"
require_dylib "SDL3_mixer" "*SDL3_mixer*.dylib"
require_dylib "SDL3_ttf" "*SDL3_ttf*.dylib"

archs="$(lipo -archs "${app_dir}/Contents/MacOS/splonks-bin")"
if [[ " ${archs} " != *" arm64 "* || " ${archs} " != *" x86_64 "* ]]; then
    echo "[verify-package] splonks-bin is not universal arm64+x86_64; archs=${archs}" >&2
    exit 1
fi

otool -L "${app_dir}/Contents/MacOS/splonks-bin"
"${app_dir}/Contents/MacOS/Splonks" \
    --check-state-fingerprint-smoke \
    --project-root "${app_dir}/Contents/Resources" \
    >/tmp/splonks-macos-package-smoke.txt
grep -q "state fingerprint smoke ok" /tmp/splonks-macos-package-smoke.txt

echo "[verify-package] ${dist_dir} ok"
