#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"

usage() {
    cat >&2 <<EOF
Usage: $0 <linux|macos|windows>

Creates a versioned release archive from an existing dist package directory.
Run the matching package and verify scripts first.

Environment:
  SPLONKS_RELEASE_VERSION   Version used in artifact names, default: ${version}
EOF
}

if [[ $# -ne 1 ]]; then
    usage
    exit 1
fi

platform="$1"
case "${platform}" in
    linux)
        package_dir="${repo_root}/dist/splonks-linux"
        archive_name="splonks-${version}-linux-x86_64.tar.gz"
        archive_path="${release_dir}/${archive_name}"
        ;;
    macos)
        package_dir="${repo_root}/dist/splonks-macos"
        archive_name="splonks-${version}-macos-universal.zip"
        archive_path="${release_dir}/${archive_name}"
        ;;
    windows)
        package_dir="${repo_root}/dist/splonks-windows"
        archive_name="splonks-${version}-windows-x86_64.zip"
        archive_path="${release_dir}/${archive_name}"
        ;;
    *)
        usage
        exit 1
        ;;
esac

if [[ ! -d "${package_dir}" ]]; then
    echo "Missing package directory: ${package_dir}" >&2
    echo "Run the matching package/verify scripts before archiving." >&2
    exit 1
fi

mkdir -p "${release_dir}"
rm -f "${archive_path}" "${archive_path}.sha256"

case "${platform}" in
    linux)
        tar -C "${repo_root}/dist" -czf "${archive_path}" "$(basename "${package_dir}")"
        ;;
    macos)
        if command -v ditto >/dev/null 2>&1; then
            ditto -c -k --keepParent "${package_dir}" "${archive_path}"
        else
            (cd "${repo_root}/dist" && zip -qry "${archive_path}" "$(basename "${package_dir}")")
        fi
        ;;
    windows)
        if command -v powershell.exe >/dev/null 2>&1; then
            powershell.exe -NoProfile -Command \
                "Compress-Archive -Path '$(cygpath -w "${package_dir}")' -DestinationPath '$(cygpath -w "${archive_path}")' -Force"
        else
            (cd "${repo_root}/dist" && zip -qry "${archive_path}" "$(basename "${package_dir}")")
        fi
        ;;
esac

if command -v sha256sum >/dev/null 2>&1; then
    (cd "${release_dir}" && sha256sum "${archive_name}" > "${archive_name}.sha256")
else
    (cd "${release_dir}" && shasum -a 256 "${archive_name}" > "${archive_name}.sha256")
fi

echo "[release] ${archive_path}"
echo "[release] ${archive_path}.sha256"
