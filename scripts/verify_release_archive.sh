#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"

usage() {
    cat >&2 <<EOF
Usage: $0 <linux|macos|windows>

Verifies a versioned release archive and its .sha256 file.

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
        archive_name="splonks-${version}-linux-x86_64.tar.gz"
        required_entries=(
            "splonks-linux/bin/splonks-cpp"
            "splonks-linux/run-splonks.sh"
            "splonks-linux/PACKAGE_MANIFEST.txt"
            "splonks-linux/lib/libSDL3.so.0"
            "splonks-linux/assets/fonts/DejaVuSans.ttf"
            "splonks-linux/data/settings.cfg"
        )
        ;;
    macos)
        archive_name="splonks-${version}-macos-universal.zip"
        required_entries=(
            "splonks-macos/Splonks.app/Contents/MacOS/Splonks"
            "splonks-macos/Splonks.app/Contents/Info.plist"
            "splonks-macos/Splonks.app/Contents/Resources/assets/fonts/DejaVuSans.ttf"
            "splonks-macos/Splonks.app/Contents/Resources/data/settings.cfg"
        )
        ;;
    windows)
        archive_name="splonks-${version}-windows-x86_64.zip"
        required_entries=(
            "splonks-windows/splonks-cpp.exe"
            "splonks-windows/run-splonks.bat"
            "splonks-windows/PACKAGE_MANIFEST.txt"
            "splonks-windows/assets/fonts/DejaVuSans.ttf"
            "splonks-windows/data/settings.cfg"
        )
        ;;
    *)
        usage
        exit 1
        ;;
esac

archive_path="${release_dir}/${archive_name}"
checksum_path="${archive_path}.sha256"

if [[ ! -f "${archive_path}" ]]; then
    echo "Missing archive: ${archive_path}" >&2
    exit 1
fi
if [[ ! -f "${checksum_path}" ]]; then
    echo "Missing checksum: ${checksum_path}" >&2
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    (cd "${release_dir}" && sha256sum -c "$(basename "${checksum_path}")")
else
    expected="$(awk '{print $1}' "${checksum_path}")"
    actual="$(shasum -a 256 "${archive_path}" | awk '{print $1}')"
    if [[ "${expected}" != "${actual}" ]]; then
        echo "Checksum mismatch for ${archive_path}" >&2
        exit 1
    fi
fi

case "${archive_name}" in
    *.tar.gz)
        listing="$(tar -tzf "${archive_path}")"
        ;;
    *.zip)
        if ! command -v unzip >/dev/null 2>&1; then
            echo "Missing unzip for archive content verification" >&2
            exit 1
        fi
        listing="$(unzip -Z1 "${archive_path}")"
        ;;
esac

for entry in "${required_entries[@]}"; do
    if ! grep -Fxq "${entry}" <<<"${listing}"; then
        echo "Missing archive entry: ${entry}" >&2
        exit 1
    fi
done

echo "[verify-release] ${archive_path} ok"
