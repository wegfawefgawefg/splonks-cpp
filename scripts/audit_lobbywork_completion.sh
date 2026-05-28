#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
gubsy_root="$(cd -- "${repo_root}/../gubsy" && pwd)"
verdict_json="${LOBBY_PLAYTEST_VERDICT_JSON:-${repo_root}/logs/lobby_human_playtest_verdict.json}"

usage() {
    printf '%s\n' \
        "Usage: scripts/audit_lobbywork_completion.sh [--skip-auto]" \
        "" \
        "Audits the lobbywork completion gate from docs/plan_3_lobbywork.md." \
        "By default this runs the focused automated checks and then requires" \
        "the filled human playtest verdict to pass." \
        "" \
        "Options:" \
        "  --skip-auto    Only audit logs/lobby_human_playtest_verdict.json."
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
    printf '[lobbywork] Splonks build\n'
    "${script_dir}/build.sh"

    printf '[lobbywork] Splonks focused Gubsy shell smokes\n'
    ctest --test-dir build --output-on-failure -R \
        "gubsy_shell_smoke|gubsy_import_smoke|gubsy_binds_smoke"

    printf '[lobbywork] Splonks real roomd smoke\n'
    "${script_dir}/validate_gubsy_roomd_live.sh"

    printf '[lobbywork] Splonks same-house lockstep live validation\n'
    "${script_dir}/validate_lockstep_live.py" \
        --launch-pair \
        --profile same-house \
        --ready-timeout 60 \
        --report-json logs/lobbywork_lockstep_validate_report.json

    printf '[lobbywork] Gubsy room smoke\n'
    (cd "${gubsy_root}" && ./scripts/room_smoke.sh)

    printf '[lobbywork] Gubsy lobby online smoke\n'
    (cd "${gubsy_root}" && ./scripts/lobby_online_smoke.sh)

    printf '[lobbywork] Gubsy rendered lobby online smoke\n'
    (cd "${gubsy_root}" && GUBSY_RENDER_SMOKE=1 ./scripts/lobby_online_smoke.sh)
fi

printf '[lobbywork] Human verdict audit\n'
"${script_dir}/summarize_lobby_human_playtest.py" --verdict-json "${verdict_json}"

printf '[lobbywork] completion audit ok\n'
