#!/usr/bin/env bash
set -euo pipefail

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
    *)
        echo "verify_package_windows.sh must run on Windows through Git Bash/MSYS/MinGW/Cygwin" >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/splonks-windows"

"${repo_root}/scripts/package_windows.sh"

required_files=(
    "${dist_dir}/splonks-cpp.exe"
    "${dist_dir}/PACKAGE_MANIFEST.txt"
    "${dist_dir}/run-splonks.bat"
    "${dist_dir}/assets/fonts/DejaVuSans.ttf"
    "${dist_dir}/assets/graphics/annotations.yaml"
    "${dist_dir}/assets/audio/annotations.yaml"
    "${dist_dir}/data/settings.cfg"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "[verify-package] missing ${path}" >&2
        exit 1
    fi
done

grep -q "^app=splonks$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^platform=windows$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^mode=release$" "${dist_dir}/PACKAGE_MANIFEST.txt"

require_dll() {
    local label="$1"
    local pattern="$2"
    if ! find "${dist_dir}" -maxdepth 1 -type f -iname "${pattern}" | grep -q .; then
        echo "[verify-package] missing ${label} DLL in ${dist_dir}" >&2
        exit 1
    fi
}

require_dll "SDL3" "*SDL3.dll"
require_dll "SDL3_image" "*SDL3_image*.dll"
require_dll "SDL3_mixer" "*SDL3_mixer*.dll"
require_dll "SDL3_ttf" "*SDL3_ttf*.dll"

(
    cd "${dist_dir}"
    PATH="${dist_dir}:${PATH}" cmd.exe /C run-splonks.bat \
        --check-state-fingerprint-smoke \
        --project-root . \
        >/tmp/splonks-windows-package-smoke.txt
)
grep -q "state fingerprint smoke ok" /tmp/splonks-windows-package-smoke.txt

echo "[verify-package] Windows batch launcher smoke ok"
echo "[verify-package] ${dist_dir} ok"
