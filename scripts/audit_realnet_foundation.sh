#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"
verdict_json="${REALNET_LAN_VERDICT_JSON:-${repo_root}/logs/realnet_lan_verdict.json}"

usage() {
    printf '%s\n' \
        "Usage: scripts/audit_realnet_foundation.sh [--skip-auto]" \
        "" \
        "Audits the Realnet foundation gate." \
        "By default this runs the focused local/same-machine/LAN-interface checks" \
        "and then requires the filled two-machine LAN verdict to pass." \
        "" \
        "Options:" \
        "  --skip-auto    Only audit logs/realnet_lan_verdict.json."
}

skip_auto=0
while (($#)); do
    case "$1" in
        --skip-auto)
            skip_auto=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

cd "${repo_root}"

if ((skip_auto == 0)); then
    printf '[realnet] Gubsy build\n'
    (cd "${gubsy_root}" && ./scripts/build.sh)

    printf '[realnet] Gubsy tests\n'
    (cd "${gubsy_root}" && ctest --test-dir build --output-on-failure)

    printf '[realnet] Gubsy room smoke\n'
    (cd "${gubsy_root}" && ./scripts/room_smoke.sh)

    printf '[realnet] Gubsy lobby online smoke\n'
    (cd "${gubsy_root}" && ./scripts/lobby_online_smoke.sh)

    printf '[realnet] Splonks build\n'
    "${script_dir}/build.sh"

    printf '[realnet] Splonks Gubsy shell smoke\n'
    "${repo_root}/build/splonks-cpp" --check-gubsy-shell-smoke

    printf '[realnet] Splonks real roomd smoke\n'
    "${script_dir}/validate_gubsy_roomd_live.sh"

    printf '[realnet] Splonks LAN-interface roomd smoke\n'
    "${script_dir}/validate_gubsy_roomd_live.sh" --lan-interface
fi

printf '[realnet] Two-machine LAN verdict audit\n'
"${script_dir}/summarize_realnet_lan_validation.py" --verdict-json "${verdict_json}"

printf '[realnet] foundation audit ok\n'
