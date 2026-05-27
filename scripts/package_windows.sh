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

SPLONKS_PRESET=package-windows "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}"

cp "${build_dir}/splonks-cpp.exe" "${dist_dir}/"
cp -a "${repo_root}/assets" "${dist_dir}/assets"
cp -a "${repo_root}/data" "${dist_dir}/data"

find "${build_dir}" -type f -name "*.dll" \
    \( -iname "SDL3*.dll" -o -iname "libpng*.dll" -o -iname "freetype*.dll" \
       -o -iname "harfbuzz*.dll" -o -iname "pluto*.dll" -o -iname "vorbis*.dll" \
       -o -iname "ogg*.dll" -o -iname "zstd*.dll" -o -iname "brotli*.dll" \
       -o -iname "bz2*.dll" -o -iname "jpeg*.dll" -o -iname "webp*.dll" \) \
    -exec cp -f {} "${dist_dir}/" \;

cat > "${dist_dir}/run-splonks.bat" <<'EOF'
@echo off
cd /d "%~dp0"
"%~dp0splonks-cpp.exe" %*
EOF

echo "[package] ${dist_dir}"
