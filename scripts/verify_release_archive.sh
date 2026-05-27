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
required_patterns=()
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
        required_patterns=(
            '^splonks-linux/lib/libSDL3\.so'
            '^splonks-linux/lib/libSDL3_image\.so'
            '^splonks-linux/lib/libSDL3_mixer\.so'
            '^splonks-linux/lib/libSDL3_ttf\.so'
        )
        ;;
    macos)
        archive_name="splonks-${version}-macos-arm64.zip"
        required_entries=(
            "splonks-macos/Splonks.app/Contents/MacOS/Splonks"
            "splonks-macos/Splonks.app/Contents/MacOS/splonks-bin"
            "splonks-macos/Splonks.app/Contents/Info.plist"
            "splonks-macos/PACKAGE_MANIFEST.txt"
            "splonks-macos/Splonks.app/Contents/Resources/assets/fonts/DejaVuSans.ttf"
            "splonks-macos/Splonks.app/Contents/Resources/data/settings.cfg"
        )
        required_patterns=(
            '^splonks-macos/Splonks\.app/Contents/Frameworks/.*SDL3.*\.dylib$'
            '^splonks-macos/Splonks\.app/Contents/Frameworks/.*SDL3_image.*\.dylib$'
            '^splonks-macos/Splonks\.app/Contents/Frameworks/.*SDL3_mixer.*\.dylib$'
            '^splonks-macos/Splonks\.app/Contents/Frameworks/.*SDL3_ttf.*\.dylib$'
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
        required_patterns=(
            '^splonks-windows/.*SDL3\.dll$'
            '^splonks-windows/.*SDL3_image.*\.dll$'
            '^splonks-windows/.*SDL3_mixer.*\.dll$'
            '^splonks-windows/.*SDL3_ttf.*\.dll$'
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

for pattern in "${required_patterns[@]}"; do
    if ! grep -Eq "${pattern}" <<<"${listing}"; then
        echo "Missing archive entry matching pattern: ${pattern}" >&2
        exit 1
    fi
done

run_extracted_smoke() {
    local temp_dir
    temp_dir="$(mktemp -d)"
    trap "rm -rf '${temp_dir}'" EXIT

    case "${archive_name}" in
        *.tar.gz)
            tar -C "${temp_dir}" -xzf "${archive_path}"
            ;;
        *.zip)
            unzip -q "${archive_path}" -d "${temp_dir}"
            ;;
    esac

    case "${platform}" in
        linux)
            "${temp_dir}/splonks-linux/run-splonks.sh" \
                --check-state-fingerprint-smoke \
                --project-root "${temp_dir}/splonks-linux" \
                >"${temp_dir}/smoke.txt"
            ;;
        macos)
            if [[ "$(uname -s)" != "Darwin" ]]; then
                echo "[verify-release] skipped macOS extracted launch on non-macOS host"
                return
            fi
            "${temp_dir}/splonks-macos/Splonks.app/Contents/MacOS/Splonks" \
                --check-state-fingerprint-smoke \
                --project-root "${temp_dir}/splonks-macos/Splonks.app/Contents/Resources" \
                >"${temp_dir}/smoke.txt"
            ;;
        windows)
            case "${OS:-}:$(uname -s)" in
                Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
                *)
                    echo "[verify-release] skipped Windows extracted launch on non-Windows host"
                    return
                    ;;
            esac
            (
                cd "${temp_dir}/splonks-windows"
                PATH="${temp_dir}/splonks-windows:${PATH}" cmd.exe /C run-splonks.bat \
                    --check-state-fingerprint-smoke \
                    --project-root . \
                    >"${temp_dir}/smoke.txt"
            )
            ;;
    esac

    grep -q "state fingerprint smoke ok" "${temp_dir}/smoke.txt"
    if [[ "${platform}" == "windows" ]]; then
        echo "[verify-release] Windows batch launcher smoke ok"
    fi
    rm -rf "${temp_dir}"
    trap - EXIT
}

run_extracted_smoke

echo "[verify-release] ${archive_path} ok"
