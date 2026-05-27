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

if ! find "${dist_dir}" -maxdepth 1 -type f -iname "SDL3*.dll" | grep -q .; then
    echo "[verify-package] missing SDL3 DLL in ${dist_dir}" >&2
    exit 1
fi

(
    cd "${dist_dir}"
    PATH="${dist_dir}:${PATH}" ./splonks-cpp.exe --check-state-fingerprint-smoke >/tmp/splonks-windows-package-smoke.txt
)
grep -q "state fingerprint smoke ok" /tmp/splonks-windows-package-smoke.txt

echo "[verify-package] ${dist_dir} ok"
