#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/splonks-linux"

"${repo_root}/scripts/package_linux.sh"

required_files=(
    "${dist_dir}/bin/splonks-cpp"
    "${dist_dir}/lib/libSDL3.so.0"
    "${dist_dir}/lib/libSDL3_image.so.0"
    "${dist_dir}/lib/libSDL3_mixer.so.0"
    "${dist_dir}/lib/libSDL3_ttf.so.0"
    "${dist_dir}/run-splonks.sh"
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

ldd_output="$(LD_LIBRARY_PATH="${dist_dir}/lib" ldd "${dist_dir}/bin/splonks-cpp")"
if grep -q "not found" <<<"${ldd_output}"; then
    echo "${ldd_output}" >&2
    exit 1
fi
for lib in libSDL3.so.0 libSDL3_image.so.0 libSDL3_mixer.so.0 libSDL3_ttf.so.0; do
    if ! grep -q "${dist_dir}/lib/${lib}" <<<"${ldd_output}"; then
        echo "[verify-package] ${lib} did not resolve from ${dist_dir}/lib" >&2
        echo "${ldd_output}" >&2
        exit 1
    fi
done

"${dist_dir}/run-splonks.sh" --check-state-fingerprint-smoke >/tmp/splonks-package-smoke.txt
grep -q "state fingerprint smoke ok" /tmp/splonks-package-smoke.txt

echo "[verify-package] ${dist_dir} ok"
