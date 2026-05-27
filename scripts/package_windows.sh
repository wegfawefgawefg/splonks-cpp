#!/usr/bin/env bash
set -euo pipefail

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
    *)
        echo "package_windows.sh must run on Windows through Git Bash/MSYS/MinGW/Cygwin" >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-windows"
dist_dir="${repo_root}/dist/splonks-windows"
source "${repo_root}/scripts/package_runtime_libs.sh"

SPLONKS_PRESET=package-windows "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}"

cp "${build_dir}/splonks-cpp.exe" "${dist_dir}/"
cp -a "${repo_root}/assets" "${dist_dir}/assets"
cp -a "${repo_root}/data" "${dist_dir}/data"

package_copy_runtime_libs_from_tree "${build_dir}" "${dist_dir}" ".dll"

cat > "${dist_dir}/run-splonks.bat" <<'EOF'
@echo off
cd /d "%~dp0"
"%~dp0splonks-cpp.exe" %*
EOF

echo "[package] ${dist_dir}"
