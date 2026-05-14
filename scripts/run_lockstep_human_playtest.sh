#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"

duration=300
summary_min_duration=240
ready_timeout=20
interval=1
no_prompt=0
skip_summary=0
verdict_json=""
initialized_verdict_json=""
launch_pair=0
launch_settle=3
init_verdict=0
profiles=()

usage() {
    printf '%s\n' \
        "Usage: scripts/run_lockstep_human_playtest.sh [options]" \
        "" \
        "Records the required lockstep human-playtest profiles, then runs the" \
        "telemetry summary audit." \
        "" \
        "Options:" \
        "  --duration SECONDS              Recording duration per profile (default: 300)" \
        "  --summary-min-duration SECONDS  Minimum duration for summary audit (default: 240)" \
        "  --ready-timeout SECONDS         Wait-ready timeout per profile (default: 20)" \
        "  --interval SECONDS              Telemetry sample interval (default: 1)" \
        "  --profile NAME                  Profile to record; repeatable. Defaults to required set." \
        "  --verdict-json PATH             Also audit a filled human verdict JSON." \
        "  --init-verdict                  Create logs/lockstep_playtest_verdict.json if missing." \
        "  --launch-pair                   Launch scripts/run_multiplayer_pair_i3.sh first." \
        "  --launch-settle SECONDS         Delay after --launch-pair before recording (default: 3)." \
        "  --no-prompt                     Do not pause before each profile." \
        "  --skip-summary                  Record only; do not run the summary audit." \
        "  -h, --help                      Show this help."
}

while (($#)); do
    case "$1" in
        --duration)
            duration="${2:?missing value for --duration}"
            shift 2
            ;;
        --summary-min-duration)
            summary_min_duration="${2:?missing value for --summary-min-duration}"
            shift 2
            ;;
        --ready-timeout)
            ready_timeout="${2:?missing value for --ready-timeout}"
            shift 2
            ;;
        --interval)
            interval="${2:?missing value for --interval}"
            shift 2
            ;;
        --profile)
            profiles+=("${2:?missing value for --profile}")
            shift 2
            ;;
        --verdict-json)
            verdict_json="${2:?missing value for --verdict-json}"
            shift 2
            ;;
        --init-verdict)
            init_verdict=1
            shift
            ;;
        --launch-pair)
            launch_pair=1
            shift
            ;;
        --launch-settle)
            launch_settle="${2:?missing value for --launch-settle}"
            shift 2
            ;;
        --no-prompt)
            no_prompt=1
            shift
            ;;
        --skip-summary)
            skip_summary=1
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

if ((${#profiles[@]} == 0)); then
    profiles=(same-house tx-ca tx-japan)
fi

cd "${repo_root}"

if ((init_verdict != 0)); then
    mkdir -p logs
    verdict_path="logs/lockstep_playtest_verdict.json"
    if [[ -e "${verdict_path}" ]]; then
        printf 'Verdict JSON already exists: %s\n' "${verdict_path}"
    else
        cp docs/plans/lockstep_human_playtest_verdict_template.json "${verdict_path}"
        printf 'Initialized verdict JSON: %s\n' "${verdict_path}"
    fi
    initialized_verdict_json="${verdict_path}"
fi

if ((launch_pair != 0)); then
    printf 'Launching local multiplayer pair...\n'
    "${script_dir}/run_multiplayer_pair_i3.sh"
    sleep "${launch_settle}"
fi

printf 'Lockstep human playtest recorder\n'
printf 'Profiles: %s\n' "${profiles[*]}"
printf 'Duration per profile: %ss\n' "${duration}"
printf 'Expected live pair: host control port 41000, peer control port 41001\n\n'

for profile in "${profiles[@]}"; do
    printf 'Profile: %s\n' "${profile}"
    printf 'Play the required checklist flow while this recorder runs.\n'
    if ((no_prompt == 0)); then
        read -r -p "Press Enter when both windows are ready for ${profile}..."
    fi
    "${script_dir}/record_lockstep_playtest.py" \
        --profile "${profile}" \
        --apply-profile \
        --wait-ready \
        --ready-timeout "${ready_timeout}" \
        --duration "${duration}" \
        --interval "${interval}"
    printf '\n'
done

if ((skip_summary != 0)); then
    printf 'Skipped summary audit.\n'
    exit 0
fi

summary_cmd=(
    "${script_dir}/summarize_lockstep_playtest.py"
    --min-duration "${summary_min_duration}"
)
for profile in "${profiles[@]}"; do
    summary_cmd+=(--required-profile "${profile}")
done
if [[ -n "${verdict_json}" ]]; then
    summary_cmd+=(--verdict-json "${verdict_json}")
fi

"${summary_cmd[@]}"

if [[ -z "${verdict_json}" ]]; then
    printf '\n'
    printf 'Telemetry audit finished. Final completion still requires human verdict JSON:\n'
    if [[ -n "${initialized_verdict_json}" ]]; then
        printf '  edit %s with real playtest booleans\n' "${initialized_verdict_json}"
        printf '  scripts/summarize_lockstep_playtest.py --verdict-json %s\n' "${initialized_verdict_json}"
    else
        printf '  cp docs/plans/lockstep_human_playtest_verdict_template.json logs/lockstep_playtest_verdict.json\n'
        printf '  edit logs/lockstep_playtest_verdict.json with real playtest booleans\n'
        printf '  scripts/summarize_lockstep_playtest.py --verdict-json logs/lockstep_playtest_verdict.json\n'
    fi
fi
