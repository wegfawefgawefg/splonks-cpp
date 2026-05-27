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

"${repo_root}/scripts/package_macos.sh"

required_files=(
    "${app_dir}/Contents/MacOS/Splonks"
    "${app_dir}/Contents/MacOS/splonks-bin"
    "${app_dir}/Contents/Info.plist"
    "${app_dir}/Contents/Resources/assets/fonts/DejaVuSans.ttf"
    "${app_dir}/Contents/Resources/assets/graphics/annotations.yaml"
    "${app_dir}/Contents/Resources/assets/audio/annotations.yaml"
    "${app_dir}/Contents/Resources/data/settings.cfg"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "[verify-package] missing ${path}" >&2
        exit 1
    fi
done

if ! find "${frameworks_dir}" -type f -name "*SDL3*.dylib" | grep -q .; then
    echo "[verify-package] missing bundled SDL3 dylib in ${frameworks_dir}" >&2
    exit 1
fi

otool -L "${app_dir}/Contents/MacOS/splonks-bin"
"${app_dir}/Contents/MacOS/Splonks" --check-state-fingerprint-smoke >/tmp/splonks-macos-package-smoke.txt
grep -q "state fingerprint smoke ok" /tmp/splonks-macos-package-smoke.txt

echo "[verify-package] ${dist_dir} ok"
