#!/usr/bin/env bash

package_runtime_lib_matches() {
    local name
    name="$(basename "$1")"
    case "${name}" in
        SDL3*|libSDL3*|*png*|*freetype*|*harfbuzz*|*pluto*|*vorbis*|*ogg*|*zstd*|*brotli*|*bz2*|*jpeg*|*webp*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

package_copy_runtime_lib() {
    local src="$1"
    local dst="$2"
    [[ -f "${src}" ]] || return 0
    package_runtime_lib_matches "${src}" || return 0
    cp -f "${src}" "${dst}/"
}

package_copy_runtime_libs_from_tree() {
    local root="$1"
    local dst="$2"
    local suffix="$3"

    [[ -d "${root}" ]] || return 0
    find "${root}" -type f -name "*${suffix}" -print0 \
        | while IFS= read -r -d '' lib; do
            package_copy_runtime_lib "${lib}" "${dst}"
        done
}

package_copy_linux_runtime_deps() {
    local exe="$1"
    local dst="$2"

    [[ -x "${exe}" ]] || return 0
    ldd "${exe}" \
        | awk '/=>/ {print $(NF - 1)}' \
        | while read -r dep; do
            package_copy_runtime_lib "${dep}" "${dst}"
        done
}

package_copy_macos_runtime_deps() {
    local exe="$1"
    local dst="$2"

    [[ -x "${exe}" ]] || return 0
    otool -L "${exe}" \
        | awk 'NR > 1 {print $1}' \
        | while read -r dep; do
            package_copy_runtime_lib "${dep}" "${dst}"
        done
}
