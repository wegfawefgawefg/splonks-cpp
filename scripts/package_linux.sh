#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-linux"
dist_dir="${repo_root}/dist/splonks-linux"
source "${repo_root}/scripts/package_runtime_libs.sh"

SPLONKS_PRESET=package-linux "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}/bin" "${dist_dir}/lib"

copy_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -e "${src}" ]]; then
        cp -a "${src}" "${dst}"
    fi
}

copy_if_exists "${build_dir}/splonks-cpp" "${dist_dir}/bin/"
copy_if_exists "${repo_root}/assets" "${dist_dir}/"
copy_if_exists "${repo_root}/data" "${dist_dir}/"

package_copy_linux_runtime_deps "${dist_dir}/bin/splonks-cpp" "${dist_dir}/lib"

cat > "${dist_dir}/run-splonks.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
cd "${root}"
exec "${root}/bin/splonks-cpp" "$@"
EOF
chmod +x "${dist_dir}/run-splonks.sh"

echo "[package] ${dist_dir}"
